/*
 * Copyright (C) 2021-2024 Huawei Device Co., Ltd.
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

#ifndef OHOS_IPC_DBINDER_BASE_INVOKER_PROCESS_H
#define OHOS_IPC_DBINDER_BASE_INVOKER_PROCESS_H

#include "dbinder_base_invoker_define.h"

namespace OHOS {

template <class T> void DBinderBaseInvoker<T>::ProcessTransaction(dbinder_transaction_data *tr, int32_t listenFd)
{
    MessageParcel data, reply;

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current == nullptr) {
        ZLOGE(LOG_LABEL, "IPCProcessSkeleton is nullptr");
        DfxReportFailEvent(DbinderErrorCode::RPC_DRIVER, RADAR_IPC_PROCESS_SKELETON_NULL, __FUNCTION__);
        return;
    }

    auto allocator = new (std::nothrow) DBinderSendAllocator();
    if (allocator == nullptr) {
        ZLOGE(LOG_LABEL, "DBinderSendAllocator failed, listenFd:%{public}d", listenFd);
        DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_SEND_ALLOCATOR_FAIL, __FUNCTION__);
        return;
    }
    if (!data.SetAllocator(allocator)) {
        ZLOGE(LOG_LABEL, "SetAllocator failed, listenFd:%{public}d", listenFd);
        DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_SET_ALLOCATOR_FAIL, __FUNCTION__);
        delete allocator;
        return;
    }
    data.ParseFrom(reinterpret_cast<uintptr_t>(tr->buffer), tr->buffer_size);
    if (!(tr->flags & MessageOption::TF_STATUS_CODE) && tr->offsets_size > 0) {
        data.InjectOffsets(reinterpret_cast<binder_uintptr_t>(reinterpret_cast<char *>(tr->buffer) + tr->offsets),
            tr->offsets_size / sizeof(binder_size_t));
    }
    uint32_t &newflags = const_cast<uint32_t &>(tr->flags);
    int isServerTraced = HitraceInvoker::TraceServerReceieve(tr->cookie, tr->code, data, newflags);

    const pid_t oldPid = GetCallerPid();
    const auto oldUid = static_cast<const uid_t>(GetCallerUid());
    const std::string oldDeviceId = GetCallerDeviceID();
    uint32_t oldStatus = GetStatus();
    int32_t oldClientFd = GetClientFd();
    const uint32_t oldTokenId = GetCallerTokenID();
    if (CheckAndSetCallerInfo(listenFd, tr->cookie) != ERR_NONE) {
        ZLOGE(LOG_LABEL, "check and set caller info failed, cmd:%{public}u listenFd:%{public}d", tr->code, listenFd);
        DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_CHECK_AND_SET_CALLER_FAIL, __FUNCTION__);
        return;
    }
    SetStatus(IRemoteInvoker::ACTIVE_INVOKER);

    const uint32_t flags = tr->flags;
    uint64_t senderSeqNumber = tr->seqNumber;
    int error = ERR_NONE;
    {
        std::lock_guard<std::mutex> lockGuard(objectMutex_);
        auto *stub = current->QueryStubByIndex(tr->cookie);
        if (stub == nullptr) {
            ZLOGE(LOG_LABEL, "stubIndex is invalid, listenFd:%{public}d seq:%{public}" PRIu64,
                listenFd, senderSeqNumber);
            DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_STUB_INVALID, __FUNCTION__);
            return;
        }
        if (!IRemoteObjectTranslateWhenRcv(reinterpret_cast<char *>(tr->buffer), tr->buffer_size, data,
            listenFd, nullptr)) {
            ZLOGE(LOG_LABEL, "translate object failed, listenFd:%{public}d seq:%{public}" PRIu64,
                listenFd, senderSeqNumber);
            DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_TRANSLATE_FAIL, __FUNCTION__);
            return;
        }

        auto *stubObject = reinterpret_cast<IPCObjectStub *>(stub);
        MessageOption option;
        option.SetFlags(flags);
        // cannot use stub any more after SendRequest because this cmd may be
        // dbinder dec ref and thus stub will be destroyed
        int error = stubObject->SendRequest(tr->code, data, reply, option);
        if (error != ERR_NONE) {
            ZLOGW(LOG_LABEL, "stub sendrequest failed, cmd:%{public}u error:%{public}d "
                "listenFd:%{public}d seq:%{public}" PRIu64, tr->code, error, listenFd, senderSeqNumber);
            // can not return;
        }
    }

    if (data.GetRawData() != nullptr) {
        ZLOGW(LOG_LABEL, "delete raw data in process skeleton, listenFd:%{public}d seq:%{public}" PRIu64,
            listenFd, senderSeqNumber);
        current->DetachRawData(listenFd);
    }
    HitraceInvoker::TraceServerSend(tr->cookie, tr->code, isServerTraced, newflags);
    if (!(flags & MessageOption::TF_ASYNC)) {
        SetSeqNum(senderSeqNumber);
        SendReply(reply, 0, error);
        SetSeqNum(0);
    }

    SetCallerPid(oldPid);
    SetCallerUid(oldUid);
    SetCallerDeviceID(oldDeviceId);
    SetStatus(oldStatus);
    SetClientFd(oldClientFd);
    SetCallerTokenID(oldTokenId);
}

template <class T> void DBinderBaseInvoker<T>::ProcessReply(dbinder_transaction_data *tr, int32_t listenFd)
{
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    if (current == nullptr) {
        ZLOGE(LOG_LABEL, "IPCProcessSkeleton is nullptr, can not wakeup thread");
        DfxReportFailEvent(DbinderErrorCode::RPC_DRIVER, RADAR_IPC_PROCESS_SKELETON_NULL, __FUNCTION__);
        return;
    }

    std::shared_ptr<ThreadMessageInfo> messageInfo = current->QueryThreadBySeqNumber(tr->seqNumber);
    if (messageInfo == nullptr) {
        ZLOGE(LOG_LABEL, "no thread waiting reply message of this seqNumber:%{public}llu listenFd:%{public}d",
            tr->seqNumber, listenFd);
        DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_SEQ_MESSAGE_NULL, __FUNCTION__);
        /* messageInfo is null, no thread need to wakeup */
        return;
    }

    /* tr->sizeOfSelf > sizeof(dbinder_transaction_data) is checked in CheckTransactionData */
    messageInfo->buffer = new (std::nothrow) unsigned char[tr->sizeOfSelf - sizeof(dbinder_transaction_data)];
    if (messageInfo->buffer == nullptr) {
        ZLOGE(LOG_LABEL, "some thread is waiting for reply message, but no memory"
            ", seqNumber:%{public}llu listenFd:%{public}d", tr->seqNumber, listenFd);
        DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_SEQ_MESSAGE_BUFFER_NULL, __FUNCTION__);
        /* wake up sender thread */
        current->WakeUpThreadBySeqNumber(tr->seqNumber, listenFd);
        return;
    }
    /* copy receive message to sender thread */
    int memcpyResult = memcpy_s(messageInfo->buffer, tr->sizeOfSelf - sizeof(dbinder_transaction_data), tr->buffer,
        tr->sizeOfSelf - sizeof(dbinder_transaction_data));
    if (memcpyResult != 0) {
        ZLOGE(LOG_LABEL, "memcpy_s failed, error:%{public}d seqNumber:%{public}llu listenFd:%{public}d",
            memcpyResult, tr->seqNumber, listenFd);
        DfxReportFailListenEvent(DbinderErrorCode::RPC_DRIVER, listenFd, RADAR_ERR_MEMCPY_DATA, __FUNCTION__);
        delete[](unsigned char *) messageInfo->buffer;
        messageInfo->buffer = nullptr;
        /* wake up sender thread even no memssage */
        current->WakeUpThreadBySeqNumber(tr->seqNumber, listenFd);
        return;
    }

    messageInfo->flags = tr->flags;
    messageInfo->bufferSize = tr->buffer_size;
    messageInfo->offsetsSize = tr->offsets_size;
    messageInfo->offsets = tr->offsets;
    messageInfo->socketId = static_cast<uint32_t>(listenFd);

    /* wake up sender thread */
    current->WakeUpThreadBySeqNumber(tr->seqNumber, listenFd);
}

template <class T>
bool DBinderBaseInvoker<T>::ProcessRawData(std::shared_ptr<T> sessionObject, MessageParcel &data, uint64_t seqNum)
{
    if (data.GetRawData() == nullptr || data.GetRawDataSize() == 0) {
        return true; // do nothing, return true
    }

    std::shared_ptr<dbinder_transaction_data> transData = nullptr;
    size_t totalSize = sizeof(dbinder_transaction_data) + data.GetRawDataSize();
    transData.reset(reinterpret_cast<dbinder_transaction_data *>(::operator new(totalSize)));
    if (transData == nullptr) {
        ZLOGE(LOG_LABEL, "fail to create raw buffer with length:%{public}zu", totalSize);
        return false;
    }

    ConstructTransData(data, *transData, totalSize, seqNum, BC_SEND_RAWDATA, 0, 0);
    int result = memcpy_s(reinterpret_cast<char *>(transData.get()) + sizeof(dbinder_transaction_data),
        totalSize - sizeof(dbinder_transaction_data), data.GetRawData(), data.GetRawDataSize());
    if (result != 0) {
        ZLOGE(LOG_LABEL, "memcpy data fail, size:%{public}zu", data.GetRawDataSize());
        return false;
    }
    result = OnSendRawData(sessionObject, transData.get(), totalSize);
    if (result != 0) {
        ZLOGE(LOG_LABEL, "fail to send raw data");
        return false;
    }
    return true;
}

template <class T>
std::shared_ptr<dbinder_transaction_data> DBinderBaseInvoker<T>::ProcessNormalData(std::shared_ptr<T> sessionObject,
    MessageParcel &data, int32_t handle, int32_t socketId, uint64_t seqNum, int cmd, __u32 code, __u32 flags,
    int status)
{
    uint32_t sendSize = ((data.GetDataSize() > 0) ? data.GetDataSize() : sizeof(binder_size_t)) +
        sizeof(struct dbinder_transaction_data) + data.GetOffsetsSize() * T::GetFlatSessionLen() +
        data.GetOffsetsSize() * sizeof(binder_size_t);

    std::shared_ptr<dbinder_transaction_data> transData = nullptr;
    transData.reset(reinterpret_cast<dbinder_transaction_data *>(::operator new(sendSize)));
    if (transData == nullptr) {
        ZLOGE(LOG_LABEL, "new buffer failed of length:%{public}u", sendSize);
        return nullptr;
    }
    ConstructTransData(data, *transData, sendSize, seqNum, cmd, code, flags);
    transData->cookie = (handle == 0) ? 0 : sessionObject->GetStubIndex();
    if (MoveMessageParcel2TransData(data, sessionObject, transData, socketId, status) != true) {
        ZLOGE(LOG_LABEL, "move parcel to transData failed, handle:%{public}d socketId:%{public}d", handle, socketId);
        return nullptr;
    }
    return transData;
}

template <class T>
std::shared_ptr<ThreadProcessInfo> DBinderBaseInvoker<T>::MakeThreadProcessInfo(int32_t socketId, const char *inBuffer,
    uint32_t size)
{
    if (inBuffer == nullptr || size < sizeof(dbinder_transaction_data) || size > SOCKET_MAX_BUFF_SIZE) {
        ZLOGE(LOG_LABEL, "buffer is null or size:%{public}u invalid, socketId:%{public}d", size, socketId);
        return nullptr;
    }

    std::shared_ptr<ThreadProcessInfo> processInfo(new ThreadProcessInfo, [](ThreadProcessInfo *ptr) {
        if (ptr != nullptr) {
            delete ptr;
            ptr = nullptr;
        }
    });
    if (processInfo == nullptr) {
        ZLOGE(LOG_LABEL, "make ThreadProcessInfo fail, socketId:%{public}d", socketId);
        return nullptr;
    }
    std::shared_ptr<char> buffer(new (std::nothrow) char[size]);
    if (buffer == nullptr) {
        ZLOGE(LOG_LABEL, "new buffer failed of length:%{public}u socketId:%{public}d", size, socketId);
        return nullptr;
    }

    int memcpyResult = memcpy_s(buffer.get(), size, inBuffer, size);
    if (memcpyResult != 0) {
        ZLOGE(LOG_LABEL, "memcpy_s failed , size:%{public}u socketId:%{public}d", size, socketId);
        return nullptr;
    }

    processInfo->listenFd = socketId;
    processInfo->packageSize = size;
    processInfo->buffer = buffer;
    return processInfo;
}

} // namespace OHOS
#endif // OHOS_IPC_DBINDER_BASE_INVOKER_PROCESS_H