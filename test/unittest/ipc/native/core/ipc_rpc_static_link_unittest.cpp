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

#include <cstdint>

#include <gtest/gtest.h>

#include "ipc_rpc_loader.h"
#include "ipc_rpc_ops.h"
#include "ipc_thread_skeleton.h"
#include "iremote_invoker.h"
#include "iremote_object.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace {
/*
 * Printed instead of a bare nullptr so a failure names the exact slot that is
 * missing when the capability table grows.
 */
const char *YesOrNo(bool value)
{
    return value ? "set" : "missing";
}
} // namespace

/*
 * These cases run against ipc_single_test, which is compiled with
 * IPC_RPC_STATIC_LINK and contains ipc_rpc_entry.cpp. They are the closed-shape
 * counterpart of IpcRpcLoaderUnitTest: they prove that the loader resolves the
 * provider inside its own image and that the table it publishes is complete,
 * without opening libipc_rpc.z.so.
 */
class IpcRpcStaticLinkTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @tc.name: ProviderEntryResolvesToTheSameTable
 * @tc.desc: Verify IpcRpcGetOps is linked into this library and that the loader
 *           publishes that very table instead of a copy.
 * @tc.type: FUNC
 */
HWTEST_F(IpcRpcStaticLinkTest, ProviderEntryResolvesToTheSameTable, TestSize.Level1)
{
    const IpcRpcOps *provider = IpcRpcGetOps();
    ASSERT_NE(provider, nullptr);
    EXPECT_EQ(IpcRpcOpsGet(), provider);
    EXPECT_EQ(IpcRpcOpsPeek(), provider);
}

/**
 * @tc.name: CapabilityTableReportsExpectedAbiVersion
 * @tc.desc: Verify the published table passes the ABI check and reports the
 *           version this build was compiled against.
 * @tc.type: FUNC
 */
HWTEST_F(IpcRpcStaticLinkTest, CapabilityTableReportsExpectedAbiVersion, TestSize.Level1)
{
    const IpcRpcOps *ops = IpcRpcOpsGet();
    ASSERT_NE(ops, nullptr);
    EXPECT_EQ(ops->abiVersion, IPC_RPC_OPS_ABI_VERSION);
}

/**
 * @tc.name: EveryCapabilitySlotIsPopulated
 * @tc.desc: Verify all nine operations are present, so the static shape does not
 *           silently ship a partially filled table.
 * @tc.type: FUNC
 */
HWTEST_F(IpcRpcStaticLinkTest, EveryCapabilitySlotIsPopulated, TestSize.Level1)
{
    const IpcRpcOps *ops = IpcRpcOpsGet();
    ASSERT_NE(ops, nullptr);
    EXPECT_STREQ(YesOrNo(ops->updateClientSession != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->startServerListener != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->shutdownSocket != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->grantPermission != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->removePermission != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->getPidAndUidFromServiceName != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->getLocalNodeDeviceId != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->shutdownListenSocket != nullptr), "set");
    EXPECT_STREQ(YesOrNo(ops->getClientFd != nullptr), "set");
}

/**
 * @tc.name: DatabusInvokerIsRegisteredInTheSameImage
 * @tc.desc: Verify the DATABUS invoker can be created in the static shape and is
 *           cached by the thread skeleton (same pointer on the second call).
 * @tc.type: FUNC
 */
HWTEST_F(IpcRpcStaticLinkTest, DatabusInvokerIsRegisteredInTheSameImage, TestSize.Level1)
{
    ASSERT_NE(IpcRpcOpsGet(), nullptr);
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetRemoteInvoker(IRemoteObject::IF_PROT_DATABUS);
    ASSERT_NE(invoker, nullptr);
    EXPECT_EQ(IPCThreadSkeleton::GetRemoteInvoker(IRemoteObject::IF_PROT_DATABUS), invoker);
}
} // namespace OHOS