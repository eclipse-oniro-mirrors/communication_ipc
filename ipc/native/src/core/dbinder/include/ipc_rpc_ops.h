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

#ifndef OHOS_IPC_IPC_RPC_OPS_H
#define OHOS_IPC_IPC_RPC_OPS_H

#include <cstdint>
#include <memory>
#include <string>

namespace OHOS {

class IRemoteInvoker;
class DBinderSessionObject;

/*
 * Private binary contract between libipc_single.z.so and libipc_rpc.z.so.
 * Bump IPC_RPC_OPS_ABI_VERSION for any field reorder, removal or signature
 * change; the loader rejects an exact mismatch instead of normalizing it.
 *
 * Registered private ABI (not a public interface):
 * - libipc_single.z.so additionally exports the core-side symbols that the
 *   migrated RPC translation units call (MessageParcel, IPCObjectProxy,
 *   IPCProcessSkeleton, InvokerRawData, DBinderSessionObject, BufferObject,
 *   HitraceInvoker). They stay in the same `1.0` version node as before, so the
 *   export count of that node grows (167 -> 235) even though the public API and
 *   its tags are unchanged.
 * - Those symbols are exported but are not a supported API (`libipc_single` keeps
 *   its `llndk`/`platformsdk`/`sasdk` tags for the unchanged public surface, and
 *   the new symbols inherit the existing `1.0` node): they are only guaranteed
 *   to work when libipc_single.z.so and libipc_rpc.z.so come from the
 *   same build and are installed into the same directory. Mixing images (or
 *   shipping only one of the two libraries) is a build/packaging error, not a
 *   supported configuration; the loader then fails closed with the pre-existing
 *   error codes instead of calling into a mismatched DSO.
 * - Version-node isolation was rejected: splitting the private symbols into a
 *   dedicated node would rename their versions for every existing consumer and
 *   change the ABI of symbols unrelated to this split. Any future change to this
 *   list must update `libipc_single_map` and `libipc_rpc_map` together.
 * - ADLT packaging is not supported: there `dli_fname` is an `adlt...:` pseudo
 *   name rather than a filesystem path, so the loader cannot bind the RPC library
 *   to the core library's own directory and fails closed (RPC unavailable, no
 *   crash, one log line). Supporting ADLT would require an explicit loader
 *   contract instead of the own-directory rule.
 */
constexpr uint32_t IPC_RPC_OPS_ABI_VERSION = 1;
constexpr const char *IPC_RPC_LIBRARY_NAME = "libipc_rpc.z.so";
constexpr const char *IPC_RPC_GET_OPS_SYMBOL = "IpcRpcGetOps";

/*
 * Mirrors softbus DBinderSoftbusClient::SOFTBUS_CLIENT_SUCCESS (= 0), which is
 * what the migrated call sites compare against; keep them in sync.
 */
constexpr int32_t IPC_RPC_SOFTBUS_SUCCESS = 0;

/*
 * Shared by the core call sites and the RPC implementation. inline avoids one
 * dynamic initializer per including translation unit.
 */
inline const std::string DBINDER_PKG_NAME = "DBinderBus";
inline const std::string DBINDER_SOCKET_NAME_PREFIX = "DBinder";
inline const std::string DBINDER_UID_PID_SEPARATOR = "_";

struct IpcRpcOps {
    /* Must equal IPC_RPC_OPS_ABI_VERSION; the loader rejects any other value. */
    uint32_t abiVersion;
    /* Returns true when the client session was bound, false on failure. */
    bool (*updateClientSession)(IRemoteInvoker *invoker, const std::shared_ptr<DBinderSessionObject> &session);
    /* Returns the listening socket id on success; SOCKET_ID_INVALID (-1) on failure. */
    int32_t (*startServerListener)(const std::string &ownName);
    /* Fire-and-forget close of a client socket; no status is reported. */
    void (*shutdownSocket)(int32_t socket);
    /* Returns IPC_RPC_SOFTBUS_SUCCESS (0) on success, negative/other on failure. */
    int32_t (*grantPermission)(int32_t uid, int32_t pid, const std::string &socketName);
    /* Returns IPC_RPC_SOFTBUS_SUCCESS (0) on success, negative/other on failure. */
    int32_t (*removePermission)(const std::string &socketName);
    /* Returns true when pid/uid were parsed out of the service name. */
    bool (*getPidAndUidFromServiceName)(const std::string &serviceName, int32_t &pid, int32_t &uid);
    /* Returns IPC_RPC_SOFTBUS_SUCCESS (0) on success, negative/other on failure. */
    int32_t (*getLocalNodeDeviceId)(const std::string &pkgName, std::string &devId);
    /* Fire-and-forget close of the listening socket; no status is reported. */
    void (*shutdownListenSocket)(int32_t socket);
    /* Returns the client fd, which may be unset before the session is established. */
    int32_t (*getClientFd)(IRemoteInvoker *invoker);
};

using IpcRpcGetOpsFunc = const IpcRpcOps *(*)();

/*
 * Provider-side entry of the private contract. In the split build only
 * libipc_rpc.z.so defines it and the loader reaches it through dlsym(); in the
 * static-link build shape the entry translation unit is compiled into
 * libipc_single.z.so and the loader calls it directly under IPC_RPC_STATIC_LINK.
 * Both shapes validate the returned table the same way.
 */
extern "C" const IpcRpcOps *IpcRpcGetOps();

} // namespace OHOS

#endif // OHOS_IPC_IPC_RPC_OPS_H