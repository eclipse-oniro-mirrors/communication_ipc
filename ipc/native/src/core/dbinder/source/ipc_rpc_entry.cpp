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

/*
 * Entry point for libipc_rpc.z.so. The library owns the migrated DBinder
 * implementation and publishes it to libipc_single.z.so through the versioned
 * C table described in ipc_rpc_ops.h.
 *
 * Constraints for code in this translation unit:
 *  - Nothing may be referenced from the core library's loader path: this file
 *    runs during dlopen() while IpcRpcOpsGet() is still inside call_once().
 *  - The table is a namespace-scope `const` object with constant initializers,
 *    so it is fully built at load time, before the first IpcRpcGetOps() call,
 *    and lives as long as the library stays mapped (RTLD_NODELETE).
 */

#include <cstdint>
#include <memory>
#include <string>

#include "databus_socket_listener.h"
#include "dbinder_databus_invoker.h"
#include "dbinder_session_object.h"
#include "dbinder_softbus_client.h"
#include "ipc_rpc_ops.h"
#include "ipc_types.h"
#include "singleton.h"

namespace OHOS {
namespace {

/*
 * Callers only ever pass the invoker they obtained from
 * IPCThreadSkeleton::GetRemoteInvoker(IF_PROT_DATABUS), and the factory
 * registers exactly one type for that protocol.
 */
DBinderDatabusInvoker *AsDatabusInvoker(IRemoteInvoker *invoker)
{
    return static_cast<DBinderDatabusInvoker *>(invoker);
}

bool UpdateClientSession(IRemoteInvoker *invoker, const std::shared_ptr<DBinderSessionObject> &session)
{
    DBinderDatabusInvoker *databusInvoker = AsDatabusInvoker(invoker);
    if (databusInvoker == nullptr) {
        return false;
    }
    return databusInvoker->UpdateClientSession(session);
}

int32_t StartServerListener(const std::string &ownName)
{
    std::shared_ptr<DatabusSocketListener> listener = DelayedSingleton<DatabusSocketListener>::GetInstance();
    if (listener == nullptr) {
        return SOCKET_ID_INVALID;
    }
    return listener->StartServerListener(ownName);
}

void ShutdownSocket(int32_t socket)
{
    std::shared_ptr<DatabusSocketListener> listener = DelayedSingleton<DatabusSocketListener>::GetInstance();
    if (listener == nullptr) {
        return;
    }
    listener->ShutdownSocket(socket);
}

int32_t GrantPermission(int32_t uid, int32_t pid, const std::string &socketName)
{
    return DBinderSoftbusClient::GetInstance().DBinderGrantPermission(uid, pid, socketName);
}

int32_t RemovePermission(const std::string &socketName)
{
    return DBinderSoftbusClient::GetInstance().DBinderRemovePermission(socketName);
}

bool GetPidAndUidFromServiceName(const std::string &serviceName, int32_t &pid, int32_t &uid)
{
    return DatabusSocketListener::GetPidAndUidFromServiceName(serviceName, pid, uid);
}

int32_t GetLocalNodeDeviceId(const std::string &pkgName, std::string &devId)
{
    return DBinderSoftbusClient::GetInstance().GetLocalNodeDeviceId(pkgName, devId);
}

void ShutdownListenSocket(int32_t socket)
{
    DBinderSoftbusClient::GetInstance().Shutdown(socket);
}

int32_t GetClientFd(IRemoteInvoker *invoker)
{
    DBinderDatabusInvoker *databusInvoker = AsDatabusInvoker(invoker);
    if (databusInvoker == nullptr) {
        /* Matches DBinderDatabusInvoker's unset clientFd_ default. */
        return 0;
    }
    return databusInvoker->GetClientFd();
}

/* Order and field set must match IpcRpcOps in ipc_rpc_ops.h exactly. */
const IpcRpcOps RPC_OPS = {
    .abiVersion = IPC_RPC_OPS_ABI_VERSION,
    .updateClientSession = UpdateClientSession,
    .startServerListener = StartServerListener,
    .shutdownSocket = ShutdownSocket,
    .grantPermission = GrantPermission,
    .removePermission = RemovePermission,
    .getPidAndUidFromServiceName = GetPidAndUidFromServiceName,
    .getLocalNodeDeviceId = GetLocalNodeDeviceId,
    .shutdownListenSocket = ShutdownListenSocket,
    .getClientFd = GetClientFd,
};

} // namespace

} // namespace OHOS

extern "C" const OHOS::IpcRpcOps *IpcRpcGetOps()
{
    return &OHOS::RPC_OPS;
}