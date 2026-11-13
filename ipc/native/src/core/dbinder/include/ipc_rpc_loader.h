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

#ifndef OHOS_IPC_IPC_RPC_LOADER_H
#define OHOS_IPC_IPC_RPC_LOADER_H

#include "ipc_rpc_ops.h"

namespace OHOS {

/*
 * Returns the RPC capability table. A failed load is cached as nullptr and is
 * never retried.
 *
 * Build shapes:
 *   - split enabled (default): the first call triggers a process-wide
 *     dlopen()/dlsym() of libipc_rpc.z.so followed by the image and table
 *     checks described below.
 *   - IPC_RPC_STATIC_LINK (split disabled): the four RPC translation units and
 *     the entry TU are compiled into this library, so the first call resolves
 *     to IpcRpcGetOps() in the same image. No dlopen, no path resolution and no
 *     image check happen; the ABI-version and table-completeness checks still
 *     run.
 * Either way every later call is a call_once fast path plus an acquire load,
 * and callers still dispatch through the table (a direct call would reintroduce
 * the link-time dependency the split removes).
 *
 * Allowed call sites: RPC entry points only (paths that are about to invoke a
 * migrated RPC implementation). Device-internal IPC hot paths
 * (SendRequestInner, MessageParcel, BinderInvoker) must not call this; they
 * never reach it today. The cost of the loaded fast path is a call_once check
 * plus an acquire load, which is negligible next to a cross-device round trip;
 * the rule is about keeping IPC-only processes from ever loading the RPC
 * library, not about shaving the call overhead inside RPC paths.
 *
 * Every call site owns its own null check and returns the error code that its
 * function already used before the split; there is deliberately no throwing or
 * default-value variant. The shutdown/destructor paths use IpcRpcOpsPeek()
 * instead.
 *
 * Loading policy: libipc_rpc.z.so is opened by the absolute path built from the
 * directory the core library itself was loaded from, never by bare name, and the
 * entry symbol resolved from it must come from that same file before its
 * capability table is trusted. Only an absolute own path is accepted: a relative
 * one would make the derived path CWD-relative and is rejected (fail closed).
 * The providing image is identified with dladdr()
 * on the resolved entry symbol (OpenHarmony musl randomizes dlopen handles, so
 * dlinfo(RTLD_DI_LINKMAP) rejects every legitimate handle). Verification accepts
 * two spellings of the same file (exact pathname, or the same st_dev/st_ino after
 * resolution) because the library may already be mapped through libdbinder's
 * DT_NEEDED using the search-path spelling, in which case the DSO keeps that
 * first pathname. A different file with the same basename still fails closed. Any
 * failure is cached: callers get nullptr and use their pre-existing error path.
 */
const IpcRpcOps *IpcRpcOpsGet();

/*
 * Returns the cached capability table without ever triggering a load. Safe to
 * use from shutdown/destructor paths.
 */
const IpcRpcOps *IpcRpcOpsPeek();

#if defined(IPC_RPC_UNIT_TEST) && !defined(IPC_RPC_STATIC_LINK)
/*
 * Test doubles for the loader. openHook/symHook replace dlopen()/dlsym();
 * pathHook replaces the own-library-directory resolution (returning nullptr
 * simulates "cannot resolve") and verifyHook replaces the entry-symbol image
 * check. The last two are optional so existing two-field initializers keep
 * working.
 *
 * These hooks only exist in the dynamic build shape: with IPC_RPC_STATIC_LINK
 * the loader calls IpcRpcGetOps() directly and there is nothing to hook. The
 * guard must match the one in ipc_rpc_loader.cpp, otherwise a target that
 * defines both macros links against declarations without definitions.
 */
struct IpcRpcTestHooks {
    void *(*openHook)(const char *libraryPath, int flags);
    void *(*symHook)(void *handle, const char *symbol);
    const char *(*pathHook)(void) = nullptr;
    bool (*verifyHook)(const void *symbol, const char *expectedPath) = nullptr;
};

void IpcRpcTestSetHooks(const IpcRpcTestHooks &hooks);
void IpcRpcTestReset();

/*
 * Exercise the production code paths that the hooks normally replace: the
 * dladdr()-based own-library resolution and the entry-symbol/file-identity
 * check. Both ignore the hooks so a test can cover them with a real mapping.
 *
 * IpcRpcTestOwnLibraryPath() returns the path of the DSO that contains this
 * loader and IpcRpcTestOwnLibraryMarker() returns an address inside that same
 * image, so the check can be exercised without dlopen()ing anything or knowing
 * whether the loader ended up in the test executable or in a test DSO.
 */
std::string IpcRpcTestResolveOwnLibraryDirectory();
std::string IpcRpcTestResolveDirectoryForPath(const char *ownPath);
std::string IpcRpcTestOwnLibraryPath();
const void *IpcRpcTestOwnLibraryMarker();
bool IpcRpcTestIsLoadedFromExpectedPath(const void *symbol, const char *expectedPath);
#endif

} // namespace OHOS

#endif // OHOS_IPC_IPC_RPC_LOADER_H