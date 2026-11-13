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

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "ipc_rpc_loader.h"
#include "ipc_rpc_ops.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace {
constexpr int32_t CONCURRENT_THREAD_COUNT = 8;
constexpr int32_t OPEN_DELAY_MILLISECONDS = 20;

bool TestUpdateClientSession(IRemoteInvoker *, const std::shared_ptr<DBinderSessionObject> &)
{
    return true;
}

int32_t TestStartServerListener(const std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

void TestShutdownSocket(int32_t)
{
}

int32_t TestGrantPermission(int32_t, int32_t, const std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

int32_t TestRemovePermission(const std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

bool TestGetPidAndUidFromServiceName(const std::string &, int32_t &pid, int32_t &uid)
{
    pid = 0;
    uid = 0;
    return true;
}

int32_t TestGetLocalNodeDeviceId(const std::string &, std::string &devId)
{
    devId.clear();
    return IPC_RPC_SOFTBUS_SUCCESS;
}

void TestShutdownListenSocket(int32_t)
{
}

int32_t TestGetClientFd(IRemoteInvoker *)
{
    return 0;
}

const IpcRpcOps *g_opsToReturn = nullptr;
std::atomic<int32_t> g_openCount(0);
std::atomic<int32_t> g_symCount(0);
bool g_failOpen = false;
bool g_failSymbol = false;
bool g_delayOpen = false;
bool g_failPathResolution = false;
bool g_pathVerificationPasses = true;
std::string g_lastOpenPath;

const IpcRpcOps *TestGetOps()
{
    return g_opsToReturn;
}

const char *TestResolveLibraryPath()
{
    if (g_failPathResolution) {
        return nullptr;
    }
    return "/system/lib64/platformsdk/libipc_rpc.z.so";
}

bool TestVerifyLibraryPath(const void *, const char *)
{
    return g_pathVerificationPasses;
}

void *TestOpenLibrary(const char *libraryPath, int)
{
    g_openCount.fetch_add(1);
    if (libraryPath != nullptr) {
        g_lastOpenPath = libraryPath;
    }
    if (g_delayOpen) {
        std::this_thread::sleep_for(std::chrono::milliseconds(OPEN_DELAY_MILLISECONDS));
    }
    if (g_failOpen) {
        return nullptr;
    }
    return reinterpret_cast<void *>(0x1);
}

void *TestResolveSymbol(void *, const char *)
{
    g_symCount.fetch_add(1);
    if (g_failSymbol) {
        return nullptr;
    }
    return reinterpret_cast<void *>(&TestGetOps);
}

const IpcRpcOps *BuildValidOps()
{
    static const IpcRpcOps ops = {
        IPC_RPC_OPS_ABI_VERSION,
        TestUpdateClientSession,
        TestStartServerListener,
        TestShutdownSocket,
        TestGrantPermission,
        TestRemovePermission,
        TestGetPidAndUidFromServiceName,
        TestGetLocalNodeDeviceId,
        TestShutdownListenSocket,
        TestGetClientFd,
    };
    return &ops;
}
} // namespace

class IpcRpcLoaderTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() override;
    void TearDown() override;
};

void IpcRpcLoaderTest::SetUpTestCase()
{
}

void IpcRpcLoaderTest::TearDownTestCase()
{
}

void IpcRpcLoaderTest::SetUp()
{
    IpcRpcTestReset();
    g_opsToReturn = BuildValidOps();
    g_openCount.store(0);
    g_symCount.store(0);
    g_failOpen = false;
    g_failSymbol = false;
    g_delayOpen = false;
    g_failPathResolution = false;
    g_pathVerificationPasses = true;
    g_lastOpenPath.clear();
    IpcRpcTestHooks hooks = { TestOpenLibrary, TestResolveSymbol, TestResolveLibraryPath, TestVerifyLibraryPath };
    IpcRpcTestSetHooks(hooks);
}

void IpcRpcLoaderTest::TearDown()
{
    IpcRpcTestReset();
}

HWTEST_F(IpcRpcLoaderTest, LoadFailureIsCachedOnce, TestSize.Level1)
{
    g_failOpen = true;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 0);
}

HWTEST_F(IpcRpcLoaderTest, LoadUsesResolvedAbsolutePath, TestSize.Level1)
{
    EXPECT_NE(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(g_lastOpenPath, "/system/lib64/platformsdk/libipc_rpc.z.so");
    EXPECT_EQ(g_openCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, UnresolvableLibraryPathIsRejected, TestSize.Level1)
{
    g_failPathResolution = true;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(g_openCount.load(), 0);
    EXPECT_EQ(g_symCount.load(), 0);
}

HWTEST_F(IpcRpcLoaderTest, PathVerificationFailureIsRejected, TestSize.Level1)
{
    g_pathVerificationPasses = false;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(IpcRpcOpsPeek(), nullptr);
    EXPECT_EQ(g_openCount.load(), 1);
    // Verification runs on the resolved entry symbol, so dlsym() must happen
    // before the image can be rejected; nothing is published on failure.
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, LoadSuccessIsCached, TestSize.Level1)
{
    const IpcRpcOps *first = IpcRpcOpsGet();
    const IpcRpcOps *second = IpcRpcOpsGet();
    EXPECT_NE(first, nullptr);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first, BuildValidOps());
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, ConcurrentFirstUseReturnsSameOps, TestSize.Level1)
{
    g_delayOpen = true;
    const IpcRpcOps *results[CONCURRENT_THREAD_COUNT] = { nullptr };
    std::thread threads[CONCURRENT_THREAD_COUNT];
    for (int32_t index = 0; index < CONCURRENT_THREAD_COUNT; ++index) {
        threads[index] = std::thread([index, &results]() {
            results[index] = IpcRpcOpsGet();
        });
    }
    for (int32_t index = 0; index < CONCURRENT_THREAD_COUNT; ++index) {
        threads[index].join();
    }
    for (int32_t index = 0; index < CONCURRENT_THREAD_COUNT; ++index) {
        EXPECT_EQ(results[index], BuildValidOps());
    }
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, AbiVersionMismatchIsRejected, TestSize.Level1)
{
    static IpcRpcOps mismatchedOps = *BuildValidOps();
    mismatchedOps.abiVersion = IPC_RPC_OPS_ABI_VERSION + 1;
    g_opsToReturn = &mismatchedOps;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, MissingSymbolIsRejected, TestSize.Level1)
{
    g_failSymbol = true;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, NullFunctionPointerIsRejected, TestSize.Level1)
{
    static IpcRpcOps incompleteOps = *BuildValidOps();
    incompleteOps.removePermission = nullptr;
    g_opsToReturn = &incompleteOps;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
}

HWTEST_F(IpcRpcLoaderTest, GetOpsNullResultIsRejected, TestSize.Level1)
{
    g_opsToReturn = nullptr;
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, PeekAfterGetReturnsSameOps, TestSize.Level1)
{
    EXPECT_EQ(IpcRpcOpsPeek(), nullptr);
    const IpcRpcOps *loaded = IpcRpcOpsGet();
    EXPECT_EQ(loaded, BuildValidOps());
    EXPECT_EQ(IpcRpcOpsPeek(), loaded);
    EXPECT_EQ(g_openCount.load(), 1);
    EXPECT_EQ(g_symCount.load(), 1);
}

HWTEST_F(IpcRpcLoaderTest, PeekDoesNotLoad, TestSize.Level1)
{
    EXPECT_EQ(IpcRpcOpsPeek(), nullptr);
    EXPECT_EQ(g_openCount.load(), 0);
    EXPECT_EQ(g_symCount.load(), 0);
}

HWTEST_F(IpcRpcLoaderTest, OwnLibraryDirectoryIsResolvedWithoutHooks, TestSize.Level1)
{
    // The loader is statically linked into the test executable here, so this
    // exercises dladdr() on the main program, whose reported name is exactly how
    // the launcher spelled argv[0]. The loader only trusts an absolute own path;
    // a relative one (e.g. "./IpcRpcLoaderUnitTest") must yield no directory so
    // production code fails closed instead of opening a CWD-relative library.
    const std::string ownPath = IpcRpcTestOwnLibraryPath();
    const std::string directory = IpcRpcTestResolveOwnLibraryDirectory();
    if (!ownPath.empty() && ownPath[0] == '/') {
        EXPECT_FALSE(directory.empty());
        EXPECT_EQ(directory[0], '/');
        EXPECT_NE(directory.find('/'), std::string::npos);
        EXPECT_NE(directory[directory.size() - 1], '/');
    } else {
        EXPECT_TRUE(directory.empty());
    }
}

HWTEST_F(IpcRpcLoaderTest, RelativeOwnLibraryPathIsRejected, TestSize.Level1)
{
    // A relative own path would turn the "absolute" dlopen path into a
    // CWD-relative one; the loader must fail closed instead.
    EXPECT_TRUE(IpcRpcTestResolveDirectoryForPath(nullptr).empty());
    EXPECT_TRUE(IpcRpcTestResolveDirectoryForPath("").empty());
    EXPECT_TRUE(IpcRpcTestResolveDirectoryForPath("libipc_single.z.so").empty());
    EXPECT_TRUE(IpcRpcTestResolveDirectoryForPath("./libipc_single.z.so").empty());
    EXPECT_TRUE(IpcRpcTestResolveDirectoryForPath("../platformsdk/libipc_single.z.so").empty());
    EXPECT_EQ(IpcRpcTestResolveDirectoryForPath("/libipc_single.z.so"), "/");
    EXPECT_EQ(IpcRpcTestResolveDirectoryForPath("/system/lib64/platformsdk/libipc_single.z.so"),
        "/system/lib64/platformsdk");
}

HWTEST_F(IpcRpcLoaderTest, PreloadedImageWithEquivalentPathIsAccepted, TestSize.Level1)
{
    // Use an address inside the image that contains the loader and the name
    // dladdr() reports for it, so the production dladdr()+stat() path is
    // exercised without opening anything.
    const std::string ownPath = IpcRpcTestOwnLibraryPath();
    const void *marker = IpcRpcTestOwnLibraryMarker();
    ASSERT_FALSE(ownPath.empty());
    ASSERT_NE(marker, nullptr);
    const size_t separator = ownPath.rfind('/');
    ASSERT_NE(separator, std::string::npos);

    EXPECT_TRUE(IpcRpcTestIsLoadedFromExpectedPath(marker, ownPath.c_str()));

    // Same file, different spelling: this is the shape seen when DT_NEEDED
    // mapped the library first and the loader derives its own absolute path.
    const std::string equivalentPath = ownPath.substr(0, separator + 1) + "./" + ownPath.substr(separator + 1);
    EXPECT_TRUE(IpcRpcTestIsLoadedFromExpectedPath(marker, equivalentPath.c_str()));

    // A different file with the expected basename must still be rejected. This
    // probe does not exist, so it stops at stat() and is covered again below by
    // a real file that reaches the st_dev/st_ino comparison.
    const std::string foreignPath = ownPath.substr(0, separator + 1) + "libipc_rpc_absent_probe.z.so";
    EXPECT_FALSE(IpcRpcTestIsLoadedFromExpectedPath(marker, foreignPath.c_str()));
    EXPECT_FALSE(IpcRpcTestIsLoadedFromExpectedPath(marker, nullptr));
    EXPECT_FALSE(IpcRpcTestIsLoadedFromExpectedPath(nullptr, ownPath.c_str()));
}

HWTEST_F(IpcRpcLoaderTest, ExistingDifferentFileIsRejected, TestSize.Level1)
{
    // Covers the "both names stat() successfully" path: the impostor is a real
    // file, so only the st_dev/st_ino comparison can reject it. The probe above
    // fails earlier, at stat(), and therefore does not cover this branch.
    const void *marker = IpcRpcTestOwnLibraryMarker();
    ASSERT_NE(marker, nullptr);
    EXPECT_FALSE(IpcRpcTestIsLoadedFromExpectedPath(marker, "/proc/self/maps"));
}
} // namespace OHOS