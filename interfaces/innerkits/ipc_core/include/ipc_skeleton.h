/*
 * Copyright (C) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_IPC_IPC_SKELETON_H
#define OHOS_IPC_IPC_SKELETON_H

#include "iremote_object.h"

namespace OHOS {
class IPCSkeleton {
public:
    IPCSkeleton() = default;
    ~IPCSkeleton() = default;

    /**
     * @brief Set the maximum number of threads.
     * @param maxThreadNum default max is 16, only if you need a customize value.
     * @return Returns <b>true</b> if the operation succeeds; return <b>false</b> Otherwise.
     * @since 9
     */
    static bool SetMaxWorkThreadNum(int maxThreadNum);

    /**
     * @brief Make current thread join to the IPC/RPC work thread pool.
     * @return void
     * @since 9
     */
    static void JoinWorkThread();

    /**
     * @brief Exit current thread from IPC/RPC work thread pool.
     * @return void
     * @since 9
     */
    static void StopWorkThread();

    /**
     * @brief Get calling selinux id of caller.
     * @return Returns the Sid of caller.
     * @since 12
     */
    static std::string GetCallingSid();

    /**
     * @brief Get calling process id of caller.
     * @return Returns the PID of caller; Returns a invalid value -1 in the rpc context.
     * @since 9
     */
    static pid_t GetCallingPid();

    /**
     * @brief Get calling process id of caller.
     * @return Returns the same init namespace PID of caller; Returns a invalid value -1 in the rpc context.
     * @since 9
     */
    static pid_t GetCallingRealPid();

    /**
     * @brief Get calling user id of caller.
     * @return Returns the UID of the caller; Returns a invalid value -1 in the rpc context.
     * @since 9
     */
    static pid_t GetCallingUid();

    /**
     * @brief Get calling token ID of caller.
     * @return Returns the token ID of caller; Returns a invalid value 0 in the rpc context.
     * @since 9
     */
    static uint32_t GetCallingTokenID();

    /**
     * @brief Get full calling token ID of caller.
     * @return Returns the full token ID of caller; Returns a invalid value 0 in the rpc context.
     * @since 9
     */
    static uint64_t GetCallingFullTokenID();

    /**
     * @brief Get the first token ID.
     * @return Returns the first token ID; Returns a invalid value 0 in the rpc context.
     * @since 9
     */
    static uint32_t GetFirstTokenID();

    /**
     * @brief Get the first full token ID.
     * @return Returns the first full token ID; Returns a invalid value 0 in the rpc context.
     * @since 9
     */
    static uint64_t GetFirstFullTokenID();

    /**
     * @brief Get the token ID of the self.
     * @return Returns the token ID.
     * @since 9
     */
    static uint64_t GetSelfTokenID();

    /**
     * @brief Get local device ID.
     * @return Returns the ID of the local device.
     * @since 9
     */
    static std::string GetLocalDeviceID();

    /**
     * @brief get calling device id.
     * @return Returns the device ID of the caller process.
     * @since 9
     */
    static std::string GetCallingDeviceID();

    /**
     * @brief Determine whether it is a local call.
     * @return Returns <b>true</b> if it is a local call; returns <b>false</b> otherwise.
     * @since 9
     */
    static bool IsLocalCalling();

    /**
     * @brief Get an IPCSkeleton instance.
     * @return Returns an IPCSkeleton instance.
     * @since 9
     */
    static IPCSkeleton &GetInstance();

    /**
     * @brief Get the context object.
     * @return Returns a context object of the IRemoteObject pointer.
     * @since 9
     */
    static sptr<IRemoteObject> GetContextObject();

    /**
     * @brief Set the context object.
     * @param object Indicates the IRemoteObject pointer object.
     * @return Returns {@link ERR_NONE} if the operation is successful; returns an error code
     * defined in {@link rpc_errno.h} otherwise.
     * @since 9
     */
    static bool SetContextObject(sptr<IRemoteObject> &object);

    /**
     * @brief Flush all pending commands.
     * @param object Indicates the IRemoteObject object.
     * @return Returns {@link ERR_NONE} if the operation is successful; returns an error code
     * defined in {@link ipc_types.h} otherwise.
     * @since 9
     */
    static int FlushCommands(IRemoteObject *object);

    /**
     * @brief reset calling identity.
     * @return Returns a string containing the UID and PID of the remote user.
     * @since 9
     */
    static std::string ResetCallingIdentity();

    /**
     * @brief Set calling identity.
     * @param identity Indicates the string containing the UID and PID of the remote user.
     * @return Returns <b>true</b> if the operation succeeds; returns <b>false</b> otherwise.
     * @since 9
     */
    static bool SetCallingIdentity(std::string &identity, bool flag = false);

    /**
     * @brief Trigger system-level IPC thread recycling, which can only be called by the samgr service,
     * other process have no permission to call this interface.
     * @return Returns <b>true</b> if the operation succeeds; returns <b>false</b> otherwise.
     * @since 9
     */
    static bool TriggerSystemIPCThreadReclaim();

    /**
     * @brief Enable or disable to recycle process-level IPC thread, thread recycling is enabled by default.
     * @param enable If the value is true, thread recycling is enabled; otherwise, thread recycling is disabled.
     * @return Returns <b>true</b> if the operation succeeds; returns <b>false</b> otherwise.
     * @since 9
     */
    static bool EnableIPCThreadReclaim(bool enable);

    /**
     * @brief Get thread invocation state.
     * @return Returns the invocation state
     * @since 20
     */
    static int32_t GetThreadInvocationState();

    /**
     * @brief Get calling token ID of caller on the peer device.
     * @return Returns the token ID of caller in the rpc context, Token ID can not be used for permission verification;
     * Returns a invalid value 0 in the ipc context.
     * @since 20
     */
    static uint32_t GetDCallingTokenID();
};

class IPCDfx {
public:
    IPCDfx() = default;
    ~IPCDfx() = default;

    using IPCProxyLimitCallback = std::function<void (uint64_t num)>;

    /**
     * @brief Block until idle ipc thread available.CAUTION: DO NOT USE IT ELSEWHERE EXPECT HICOLLIE!
     * @return void
     * @since 9
     */
    static void BlockUntilThreadAvailable();

    /**
     * @brief Set IPC proxy limit and callbakcC.AUTION: DO NOT USE IT ELSEWHERE EXPECT HICOLLIE!
     * @return void
     * @since 9
     */
    static bool SetIPCProxyLimit(uint64_t num, IPCProxyLimitCallback callback);
};
} // namespace OHOS
#endif // OHOS_IPC_IPC_SKELETON_H
