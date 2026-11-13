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
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "ipc_rpc_loader.h"
#include "ipc_rpc_ops.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace {
/*
 * This binary links the loader in its static shape (IPC_RPC_STATIC_LINK) and
 * provides the entry itself, so the failure branch can be exercised without
 * dlopen(). It intentionally ships a table with one missing slot: the loader
 * must reject it, cache the rejection and never ask for the table again.
 */
int32_t providerCalls = 0;

bool DummyUpdateClientSession(IRemoteInvoker *, const std::shared_ptr<DBinderSessionObject> &)
{
    return true;
}

int32_t DummyStartServerListener(const std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

void DummyShutdownSocket(int32_t)
{
}

int32_t DummyGrantPermission(int32_t, int32_t, const std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

int32_t DummyRemovePermission(const std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

bool DummyGetPidAndUidFromServiceName(const std::string &, int32_t &, int32_t &)
{
    return true;
}

int32_t DummyGetLocalNodeDeviceId(const std::string &, std::string &)
{
    return IPC_RPC_SOFTBUS_SUCCESS;
}

int32_t DummyGetClientFd(IRemoteInvoker *)
{
    return -1;
}

/*
 * Designated initializers, same as the production table in ipc_rpc_entry.cpp.
 * Positional initialization would silently shift every slot if a field is ever
 * inserted, so the "missing slot" below would stop being the slot under test
 * while this case kept passing.
 */
const IpcRpcOps INCOMPLETE_TABLE = {
    .abiVersion = IPC_RPC_OPS_ABI_VERSION,
    .updateClientSession = DummyUpdateClientSession,
    .startServerListener = DummyStartServerListener,
    .shutdownSocket = DummyShutdownSocket,
    .grantPermission = DummyGrantPermission,
    .removePermission = DummyRemovePermission,
    .getPidAndUidFromServiceName = DummyGetPidAndUidFromServiceName,
    .getLocalNodeDeviceId = DummyGetLocalNodeDeviceId,
    .shutdownListenSocket = nullptr, // deliberately missing
    .getClientFd = DummyGetClientFd,
};
} // namespace

extern "C" const IpcRpcOps *IpcRpcGetOps()
{
    ++providerCalls;
    return &INCOMPLETE_TABLE;
}

class IpcRpcStaticLinkFailureTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @tc.name: IncompleteTableIsRejectedAndNotRetried
 * @tc.desc: Verify the static loader shape applies the same completeness check as
 *           the dynamic one, caches the failure as a null table, and calls the
 *           provider only once even when IpcRpcOpsGet() is asked again.
 * @tc.type: FUNC
 */
HWTEST_F(IpcRpcStaticLinkFailureTest, IncompleteTableIsRejectedAndNotRetried, TestSize.Level1)
{
    EXPECT_EQ(providerCalls, 0);
    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(providerCalls, 1);
    EXPECT_EQ(IpcRpcOpsPeek(), nullptr);

    EXPECT_EQ(IpcRpcOpsGet(), nullptr);
    EXPECT_EQ(providerCalls, 1);
    EXPECT_EQ(IpcRpcOpsPeek(), nullptr);
}
} // namespace OHOS