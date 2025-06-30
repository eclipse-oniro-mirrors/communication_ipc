/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#include "ipc_skeleton.h"

#include <cstdint>
#include <sys/types.h>

#include "access_token_adapter.h"
#include "iosfwd"
#include "ipc_process_skeleton.h"
#include "ipc_thread_skeleton.h"
#include "ipc_types.h"
#include "iremote_invoker.h"
#include "iremote_object.h"
#include "refbase.h"
#include "selinux/selinux.h"
#include "string"
#include "unistd.h"
#ifdef FFRT_IPC_ENABLE
#include "c/ffrt_ipc.h"
#endif

namespace OHOS {
#ifdef CONFIG_IPC_SINGLE
using namespace IPC_SINGLE;
#endif

static std::string GetSid()
{
    char *con = nullptr;
    int ret = getcon(&con);
    if (ret < 0) {
        return "";
    }
    std::string context = con;
    freecon(con);
    return context;
}

// LCOV_EXCL_START
void IPCSkeleton::JoinWorkThread()
{
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    if (current != nullptr) {
        current->JoinWorkThread(IRemoteObject::IF_PROT_DEFAULT);
    }
}
// LCOV_EXCL_STOP

void IPCSkeleton::StopWorkThread()
{
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    if (current != nullptr) {
        current->StopWorkThread(IRemoteObject::IF_PROT_DEFAULT);
    }
}

bool IPCSkeleton::SetContextObject(sptr<IRemoteObject> &object)
{
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current != nullptr) {
        return current->SetRegistryObject(object);
    }
    return false;
}

// LCOV_EXCL_START
sptr<IRemoteObject> IPCSkeleton::GetContextObject()
{
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current != nullptr) {
        return current->GetRegistryObject();
    }
    return nullptr;
}
// LCOV_EXCL_STOP

bool IPCSkeleton::SetMaxWorkThreadNum(int maxThreadNum)
{
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current != nullptr) {
        // first thread have started at IPCProcessSkeleton instances
        return current->SetMaxWorkThread(maxThreadNum);
    }
    return false;
}

// LCOV_EXCL_START
std::string IPCSkeleton::GetCallingSid()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetCallerSid();
    }
    return GetSid();
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
pid_t IPCSkeleton::GetCallingPid()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetCallerPid();
    }
    return getpid();
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
pid_t IPCSkeleton::GetCallingRealPid()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetCallerRealPid();
    }
    return getprocpid();
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
pid_t IPCSkeleton::GetCallingUid()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetCallerUid();
    }
    return getuid();
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
uint32_t IPCSkeleton::GetCallingTokenID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return static_cast<uint32_t>(invoker->GetCallerTokenID());
    }
    return static_cast<uint32_t>(GetSelfTokenID());
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
uint64_t IPCSkeleton::GetCallingFullTokenID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetCallerTokenID();
    }
    return GetSelfTokenID();
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
uint64_t IPCSkeleton::GetSelfTokenID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetDefaultInvoker();
    if (invoker != nullptr) {
        return invoker->GetSelfTokenID();
    }
    return 0;
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
uint32_t IPCSkeleton::GetFirstTokenID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return static_cast<uint32_t>(invoker->GetFirstCallerTokenID());
    }
    invoker = IPCThreadSkeleton::GetDefaultInvoker();
    if (invoker != nullptr) {
        return static_cast<uint32_t>(invoker->GetSelfFirstCallerTokenID());
    }
    return 0;
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
uint64_t IPCSkeleton::GetFirstFullTokenID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetFirstCallerTokenID();
    }
    invoker = IPCThreadSkeleton::GetDefaultInvoker();
    if (invoker != nullptr) {
        return invoker->GetSelfFirstCallerTokenID();
    }
    return 0;
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
std::string IPCSkeleton::GetLocalDeviceID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetLocalDeviceID();
    }
    return "";
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
std::string IPCSkeleton::GetCallingDeviceID()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->GetCallerDeviceID();
    }
    return "";
}
// LCOV_EXCL_STOP

IPCSkeleton &IPCSkeleton::GetInstance()
{
    static IPCSkeleton skeleton;
    return skeleton;
}

// LCOV_EXCL_START
bool IPCSkeleton::IsLocalCalling()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->IsLocalCalling();
    }
    return true;
}
// LCOV_EXCL_STOP

int IPCSkeleton::FlushCommands(IRemoteObject *object)
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetProxyInvoker(object);
    if (invoker == nullptr) {
        return IPC_SKELETON_NULL_OBJECT_ERR;
    }

#ifdef FFRT_IPC_ENABLE
    IPCObjectProxy *proxy = reinterpret_cast<IPCObjectProxy *>(object);
    bool isBinderInvoker = (proxy->GetProto() == IRemoteObject::IF_PROT_BINDER);
    if (isBinderInvoker) {
        ffrt_this_task_set_legacy_mode(true);
    }
#endif
    int ret = invoker->FlushCommands(object);
#ifdef FFRT_IPC_ENABLE
    if (isBinderInvoker) {
        ffrt_this_task_set_legacy_mode(false);
    }
#endif
    return ret;
}

// LCOV_EXCL_START
std::string IPCSkeleton::ResetCallingIdentity()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->ResetCallingIdentity();
    }
    return "";
}
// LCOV_EXCL_STOP

bool IPCSkeleton::SetCallingIdentity(std::string &identity, bool flag)
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetActiveInvoker();
    if (invoker != nullptr) {
        return invoker->SetCallingIdentity(identity, flag);
    }

    return true;
}

// LCOV_EXCL_START
bool IPCSkeleton::TriggerSystemIPCThreadReclaim()
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetDefaultInvoker();
    if (invoker != nullptr) {
        return invoker->TriggerSystemIPCThreadReclaim();
    }
    return false;
}
// LCOV_EXCL_STOP

bool IPCSkeleton::EnableIPCThreadReclaim(bool enable)
{
    IRemoteInvoker *invoker = IPCThreadSkeleton::GetDefaultInvoker();
    if (invoker != nullptr) {
        return invoker->EnableIPCThreadReclaim(enable);
    }
    return false;
}

// LCOV_EXCL_START
int32_t IPCSkeleton::GetThreadInvocationState()
{
    return IPCThreadSkeleton::GetThreadInvocationState();
}
// LCOV_EXCL_STOP

// LCOV_EXCL_START
void IPCDfx::BlockUntilThreadAvailable()
{
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current != nullptr) {
        current->BlockUntilThreadAvailable();
    }
}
// LCOV_EXCL_STOP

bool IPCDfx::SetIPCProxyLimit(uint64_t num, IPCProxyLimitCallback callback)
{
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current != nullptr) {
        return current->SetIPCProxyLimit(num, callback);
    }
    return false;
}
} // namespace OHOS
