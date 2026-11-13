/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ipc_rpc_loader.h"

#include <dlfcn.h>
#include <sys/stat.h>

#include <atomic>
#include <mutex>
#include <new>
#include <string>

#ifdef IPC_RPC_UNIT_TEST
#include <cstdio>
#else
#include "ipc_debug.h"
#include "log_tags.h"
#endif

namespace OHOS {

#ifdef IPC_RPC_UNIT_TEST
#define IPC_RPC_LOG_LOAD_FAIL(path, reason) \
    std::fprintf(stderr, "[IpcRpcLoader] load %s failed, reason: %s\n", (path), (reason))
#else
using namespace OHOS::HiviewDFX;
static constexpr HiLogLabel RPC_LOG_LABEL = { LOG_CORE, LOG_ID_IPC_COMMON, "IpcRpcLoader" };
#define IPC_RPC_LOG_LOAD_FAIL(path, reason) \
    ZLOGE(RPC_LOG_LABEL, "load %{public}s failed, reason: %{public}s", (path), (reason))
#endif

namespace {

/* RTLD_LOCAL is added by OpenRpcLibrary(); tests reuse the same core flags. */

std::once_flag g_loadOnce;
std::atomic<const IpcRpcOps *> g_ops { nullptr };

#ifdef IPC_RPC_UNIT_TEST
IpcRpcTestHooks g_testHooks { nullptr, nullptr };
#endif

bool IsOpsComplete(const IpcRpcOps *ops)
{
    return ops->updateClientSession != nullptr && ops->startServerListener != nullptr &&
        ops->shutdownSocket != nullptr && ops->grantPermission != nullptr &&
        ops->removePermission != nullptr && ops->getPidAndUidFromServiceName != nullptr &&
        ops->getLocalNodeDeviceId != nullptr && ops->shutdownListenSocket != nullptr &&
        ops->getClientFd != nullptr;
}

/*
 * Shared by both link shapes. A table is only published when it is
 * version-matched and complete, so callers never see a partially filled table
 * (which would silently turn a missing entry into a null-pointer call).
 */
bool CheckCapabilityTable(const IpcRpcOps *ops, std::string &failure)
{
    if (ops == nullptr) {
        failure = "IpcRpcGetOps returned null";
        return false;
    }
    if (ops->abiVersion != IPC_RPC_OPS_ABI_VERSION) {
        failure = "capability table ABI version mismatch";
        return false;
    }
    if (!IsOpsComplete(ops)) {
        failure = "capability table has null entries";
        return false;
    }
    return true;
}

/*
 * Dynamic shape only: when the split is enabled the capability table comes from
 * a companion libipc_rpc.z.so, so the loader has to resolve, open and verify
 * that image. With IPC_RPC_STATIC_LINK the entry TU is linked into this library
 * and none of the code below is reachable.
 */
#ifndef IPC_RPC_STATIC_LINK

/* RTLD_LOCAL is added by OpenRpcLibrary(); tests reuse the same core flags. */
constexpr int32_t IPC_RPC_OPEN_FLAGS = RTLD_NOW | RTLD_NODELETE;

/* Appends dlerror() detail so the single cached failure log stays actionable. */
std::string DescribeLoaderError(const char *fallback)
{
    const char *detail = dlerror();
    if (detail == nullptr || detail[0] == '\0') {
        return std::string(fallback);
    }
    return std::string(fallback) + ": " + detail;
}

/*
 * Marker whose address lives in this DSO. dladdr() maps it back to the file the
 * core library was actually loaded from, which is the directory that ships the
 * matching libipc_rpc.z.so.
 */
void IpcRpcOwnLibraryMarker()
{
}

std::string GetOwnLibraryPath()
{
    Dl_info info {};
    if (dladdr(reinterpret_cast<void *>(IpcRpcOwnLibraryMarker), &info) == 0 || info.dli_fname == nullptr) {
        return std::string();
    }
    return std::string(info.dli_fname);
}

/*
 * Only an absolute own path is trusted. A relative dli_fname (relative
 * LD_LIBRARY_PATH entry, bare-name load) would make the derived directory
 * relative to the process CWD, so the "absolute" dlopen path could still be
 * satisfied by a file in an attacker-controlled working directory. Rejecting it
 * fails closed: no RPC library is loaded and callers keep their pre-existing
 * error codes.
 */
std::string DirectoryOfLibraryPath(const std::string &ownPath)
{
    if (ownPath.empty() || ownPath[0] != '/') {
        return std::string();
    }
    const size_t separator = ownPath.rfind('/');
    if (separator == std::string::npos) {
        return std::string();
    }
    if (separator == 0) {
        return std::string("/");
    }
    return ownPath.substr(0, separator);
}

std::string GetOwnLibraryDirectory()
{
    return DirectoryOfLibraryPath(GetOwnLibraryPath());
}

std::string JoinLibraryPath(const std::string &directory, const char *libraryName)
{
    if (directory.empty()) {
        return std::string();
    }
    if (directory[directory.size() - 1] == '/') {
        return directory + libraryName;
    }
    return directory + "/" + libraryName;
}

/*
 * The RPC library is loaded by absolute path derived from the core library's own
 * location, never by bare name: a bare name goes through the loader search path
 * and can be satisfied by LD_LIBRARY_PATH or a directory that precedes
 * platformsdk. An empty result means "cannot resolve", and the caller fails
 * closed instead of falling back to a search-path load.
 */
std::string ResolveRpcLibraryPath()
{
#ifdef IPC_RPC_UNIT_TEST
    if (g_testHooks.pathHook != nullptr) {
        const char *hookPath = g_testHooks.pathHook();
        return hookPath == nullptr ? std::string() : std::string(hookPath);
    }
#endif
    return JoinLibraryPath(GetOwnLibraryDirectory(), IPC_RPC_LIBRARY_NAME);
}

/* True when both names resolve to the same regular file. Different spellings of
 * one file ("/a/b.so" vs "/a/./b.so", symlinks) pass; a different file with the
 * same basename, even from a directory that precedes ours in the search path,
 * fails because it has another st_dev/st_ino. */
bool NamesSameFile(const std::string &left, const std::string &right)
{
    struct stat leftStat {};
    struct stat rightStat {};
    if (stat(left.c_str(), &leftStat) != 0 || stat(right.c_str(), &rightStat) != 0) {
        return false;
    }
    return leftStat.st_dev == rightStat.st_dev && leftStat.st_ino == rightStat.st_ino;
}

/*
 * An absolute pathname is a request, not a guarantee: dlopen() can return an
 * already mapped DSO that was first loaded from somewhere else, and dlsym() can
 * be satisfied by symbol interposition. Verify the image that actually provides
 * the entry symbol, i.e. the code that is about to be called.
 *
 * dladdr() is used instead of dlinfo(RTLD_DI_LINKMAP) deliberately: OpenHarmony's
 * musl is built with HANDLE_RANDOMIZATION (every non-asan/tsan build), so
 * dlopen() returns an opaque random handle instead of a `struct dso *`. The stock
 * dlinfo() validates the handle against the loaded-DSO list and fails with
 * "Invalid library handle" for such handles, which would reject every legitimate
 * load. dladdr() takes a real code address, works with randomized handles, and
 * names the object that will actually be invoked.
 *
 * The exact-name comparison alone is not sufficient in production: when
 * libdbinder's DT_NEEDED loads the RPC library first, the DSO keeps the
 * search-path spelling used by that first load, while the loader asks for the
 * absolute path derived from the core library. Both name one file, so the check
 * falls back to file identity instead of rejecting a legitimately pre-loaded
 * library for the rest of the process lifetime.
 */
bool VerifyLoadedImage(const void *symbol, const std::string &expectedPath)
{
    Dl_info info {};
    if (symbol == nullptr || dladdr(symbol, &info) == 0 || info.dli_fname == nullptr || info.dli_fname[0] == '\0') {
        return false;
    }
    const std::string loadedPath(info.dli_fname);
    if (expectedPath == loadedPath) {
        return true;
    }
    return NamesSameFile(expectedPath, loadedPath);
}

bool IsLoadedFromExpectedPath(const void *symbol, const std::string &expectedPath)
{
#ifdef IPC_RPC_UNIT_TEST
    if (g_testHooks.verifyHook != nullptr) {
        return g_testHooks.verifyHook(symbol, expectedPath.c_str());
    }
#endif
    return VerifyLoadedImage(symbol, expectedPath);
}

void *OpenRpcLibrary(const std::string &expectedPath, std::string &failure)
{
    void *handle = nullptr;
#ifdef IPC_RPC_UNIT_TEST
    if (g_testHooks.openHook != nullptr) {
        handle = g_testHooks.openHook(expectedPath.c_str(), IPC_RPC_OPEN_FLAGS);
        if (handle == nullptr) {
            failure = "dlopen failed";
            return nullptr;
        }
    } else
#endif
    {
        handle = dlopen(expectedPath.c_str(), IPC_RPC_OPEN_FLAGS | RTLD_LOCAL);
        if (handle == nullptr) {
            failure = DescribeLoaderError("dlopen failed");
            return nullptr;
        }
    }
    return handle;
}

#endif // IPC_RPC_STATIC_LINK

/*
 * Split disabled (IPC_RPC_STATIC_LINK): ipc_rpc_entry.cpp is compiled into this
 * library, so the table is reached by a direct call. There is no path to
 * resolve, nothing to dlopen and no image to verify. The ABI and completeness
 * checks are the same ones the dynamic shape uses, so a broken table fails the
 * same way in both build shapes.
 */
#ifdef IPC_RPC_STATIC_LINK
void LoadOnce()
{
    std::string failure;
    const IpcRpcOps *ops = IpcRpcGetOps();
    if (!CheckCapabilityTable(ops, failure)) {
        IPC_RPC_LOG_LOAD_FAIL("<static link>", failure.c_str());
        g_ops.store(nullptr, std::memory_order_release);
        return;
    }
    g_ops.store(ops, std::memory_order_release);
}
#else
// Runs at most once per process; every failure is cached as a null capability
// table so later callers neither retry the load nor emit another log line.
//
// Reentrancy: this body runs inside std::call_once while the RPC library's own
// static initializers may still be executing. IpcRpcGetOps() must therefore
// never reach IpcRpcOpsGet() (directly or via a static initializer), otherwise
// the same call_once would deadlock. The handle is deliberately never
// dlclose()d: RTLD_NODELETE keeps the library mapped for the process lifetime
// and Peek() consumers may still call into the table during shutdown. That also
// covers the path-verification failure, where the rejected image stays mapped
// but is never called into.
void LoadOnce()
{
    std::string failure;
    void *handle = nullptr;
    const std::string expectedPath = ResolveRpcLibraryPath();

    if (expectedPath.empty()) {
        failure = "cannot resolve the directory of the core library";
    } else {
        handle = OpenRpcLibrary(expectedPath, failure);
    }

    void *symbol = nullptr;
    if (failure.empty()) {
#ifdef IPC_RPC_UNIT_TEST
        if (g_testHooks.symHook != nullptr) {
            symbol = g_testHooks.symHook(handle, IPC_RPC_GET_OPS_SYMBOL);
            if (symbol == nullptr) {
                failure = "dlsym IpcRpcGetOps failed";
            }
        } else
#endif
        {
            symbol = dlsym(handle, IPC_RPC_GET_OPS_SYMBOL);
            if (symbol == nullptr) {
                failure = DescribeLoaderError("dlsym IpcRpcGetOps failed");
            }
        }
    }

    const IpcRpcOps *ops = nullptr;
    if (failure.empty() && !IsLoadedFromExpectedPath(symbol, expectedPath)) {
        failure = "resolved entry is not provided by the expected library";
    }

    if (failure.empty()) {
        IpcRpcGetOpsFunc getOps = reinterpret_cast<IpcRpcGetOpsFunc>(symbol);
        ops = getOps();
        if (!CheckCapabilityTable(ops, failure)) {
            ops = nullptr;
        }
    }

    if (!failure.empty()) {
        const char *logPath = expectedPath.empty() ? "<unresolved>" : expectedPath.c_str();
        IPC_RPC_LOG_LOAD_FAIL(logPath, failure.c_str());
        g_ops.store(nullptr, std::memory_order_release);
        return;
    }
    g_ops.store(ops, std::memory_order_release);
}
#endif // IPC_RPC_STATIC_LINK

} // namespace

const IpcRpcOps *IpcRpcOpsGet()
{
    std::call_once(g_loadOnce, LoadOnce);
    return g_ops.load(std::memory_order_acquire);
}

const IpcRpcOps *IpcRpcOpsPeek()
{
    return g_ops.load(std::memory_order_acquire);
}

/*
 * The loader unit test always builds the dynamic shape (it needs the hooks), so
 * these entry points are not compiled when the static-link shape is selected.
 */
#if defined(IPC_RPC_UNIT_TEST) && !defined(IPC_RPC_STATIC_LINK)
void IpcRpcTestSetHooks(const IpcRpcTestHooks &hooks)
{
    g_testHooks = hooks;
}

void IpcRpcTestReset()
{
    g_ops.store(nullptr, std::memory_order_release);
    g_loadOnce.~once_flag();
    new (&g_loadOnce) std::once_flag();
}

std::string IpcRpcTestResolveOwnLibraryDirectory()
{
    return GetOwnLibraryDirectory();
}

std::string IpcRpcTestResolveDirectoryForPath(const char *ownPath)
{
    if (ownPath == nullptr) {
        return std::string();
    }
    return DirectoryOfLibraryPath(std::string(ownPath));
}

std::string IpcRpcTestOwnLibraryPath()
{
    return GetOwnLibraryPath();
}

bool IpcRpcTestIsLoadedFromExpectedPath(const void *symbol, const char *expectedPath)
{
    if (expectedPath == nullptr) {
        return false;
    }
    return VerifyLoadedImage(symbol, std::string(expectedPath));
}

const void *IpcRpcTestOwnLibraryMarker()
{
    return reinterpret_cast<const void *>(IpcRpcOwnLibraryMarker);
}
#endif

} // namespace OHOS