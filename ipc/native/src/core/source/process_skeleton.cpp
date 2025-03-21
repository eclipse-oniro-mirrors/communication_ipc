/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include "process_skeleton.h"

#include <cinttypes>

#include "binder_connector.h"
#include "log_tags.h"
#include "ipc_debug.h"
#include "string_ex.h"

namespace OHOS {
static constexpr OHOS::HiviewDFX::HiLogLabel LOG_LABEL = { LOG_CORE, LOG_ID_IPC_COMMON, "ProcessSkeleton" };
static constexpr uint64_t DEAD_OBJECT_TIMEOUT = 20 * (60 * 1000); // min
static constexpr uint64_t DEAD_OBJECT_CHECK_INTERVAL = 11 * (60 * 1000); // min

ProcessSkeleton* ProcessSkeleton::instance_ = nullptr;
std::mutex ProcessSkeleton::mutex_;
ProcessSkeleton::DestroyInstance ProcessSkeleton::destroyInstance_;
std::atomic<bool> ProcessSkeleton::exitFlag_ = false;

ProcessSkeleton* ProcessSkeleton::GetInstance()
{
    if ((instance_ == nullptr) && !exitFlag_) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        if ((instance_ == nullptr) && !exitFlag_) {
            instance_ = new (std::nothrow) ProcessSkeleton();
            if (instance_ == nullptr) {
                ZLOGE(LOG_LABEL, "create ProcessSkeleton object failed");
                return nullptr;
            }
        }
    }
    return instance_;
}

ProcessSkeleton::~ProcessSkeleton()
{
    ZLOGW(LOG_LABEL, "destroy");
    std::lock_guard<std::mutex> lockGuard(mutex_);
    exitFlag_ = true;
    {
        std::unique_lock<std::shared_mutex> objLock(objMutex_);
        objects_.clear();
        isContainStub_.clear();
    }
}

sptr<IRemoteObject> ProcessSkeleton::GetRegistryObject()
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, nullptr);
    std::lock_guard<std::mutex> lockGuard(mutex_);
    return registryObject_;
}

void ProcessSkeleton::SetRegistryObject(sptr<IRemoteObject> &object)
{
    CHECK_INSTANCE_EXIT(exitFlag_);
    std::lock_guard<std::mutex> lockGuard(mutex_);
    registryObject_ = object;
}

void ProcessSkeleton::SetSamgrFlag(bool flag)
{
    isSamgr_ = flag;
}

bool ProcessSkeleton::GetSamgrFlag()
{
    return isSamgr_;
}

bool ProcessSkeleton::IsContainsObject(IRemoteObject *object)
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    // check whether it is a valid IPCObjectStub object.
    std::shared_lock<std::shared_mutex> lockGuard(objMutex_);
    auto it = isContainStub_.find(object);
    if (it != isContainStub_.end()) {
        return it->second;
    }

    return false;
}

bool ProcessSkeleton::DetachObject(IRemoteObject *object, const std::u16string &descriptor)
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    std::unique_lock<std::shared_mutex> lockGuard(objMutex_);
    (void)isContainStub_.erase(object);

    // This handle may have already been replaced with a new IPCObjectProxy,
    // if someone failed the AttemptIncStrong.
    auto iterator = objects_.find(descriptor);
    if (iterator != objects_.end()) {
        objects_.erase(iterator);
        ZLOGD(LOG_LABEL, "erase desc:%{public}s", Str16ToStr8(descriptor).c_str());
        return true;
    }
    ZLOGD(LOG_LABEL, "not found, desc:%{public}s maybe has been updated", Str16ToStr8(descriptor).c_str());
    return false;
}

bool ProcessSkeleton::AttachObject(IRemoteObject *object, const std::u16string &descriptor, bool lockFlag)
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    std::unique_lock<std::shared_mutex> lockGuard(objMutex_, std::defer_lock);
    if (lockFlag) {
        lockGuard.lock();
    }
    (void)isContainStub_.insert(std::pair<IRemoteObject *, bool>(object, true));

    if (descriptor.empty()) {
        ZLOGE(LOG_LABEL, "descriptor is null");
        return false;
    }
    // If attemptIncStrong failed, old proxy might still exist, replace it with the new proxy.
    wptr<IRemoteObject> wp = object;
    auto result = objects_.insert_or_assign(descriptor, wp);
    ZLOGD(LOG_LABEL, "attach desc:%{public}s inserted:%{public}d", Str16ToStr8(descriptor).c_str(), result.second);
    return result.second;
}

sptr<IRemoteObject> ProcessSkeleton::QueryObject(const std::u16string &descriptor, bool lockFlag)
{
    sptr<IRemoteObject> result = nullptr;
    if (descriptor.empty()) {
        ZLOGE(LOG_LABEL, "descriptor is null");
        return result;
    }

    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, nullptr);
    std::shared_lock<std::shared_mutex> lockGuard(objMutex_, std::defer_lock);
    if (lockFlag) {
        lockGuard.lock();
    }
    IRemoteObject *remoteObject = nullptr;
    auto it = objects_.find(descriptor);
    if (it != objects_.end()) {
        // Life-time of IPCObjectProxy is extended to WEAK
        // now it's weak reference counted, so it's safe to get raw pointer
        remoteObject = it->second.GetRefPtr();
    }
    if (remoteObject == nullptr || !remoteObject->AttemptIncStrong(this)) {
        return result;
    }
    result = remoteObject;
    result->CheckAttemptAcquireSet(this);
    
    return result;
}

bool ProcessSkeleton::LockObjectMutex()
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    objMutex_.lock();
    return true;
}

bool ProcessSkeleton::UnlockObjectMutex()
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    objMutex_.unlock();
    return true;
}

bool ProcessSkeleton::AttachDeadObject(IRemoteObject *object, DeadObjectInfo& objInfo)
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    std::unique_lock<std::shared_mutex> lockGuard(deadObjectMutex_);
    auto result = deadObjectRecord_.insert_or_assign(object, objInfo);
    ZLOGD(LOG_LABEL, "%{public}zu handle:%{public}d desc:%{public}s inserted:%{public}d",
        reinterpret_cast<uintptr_t>(object), objInfo.handle, Str16ToStr8(objInfo.desc).c_str(), result.second);
    DetachTimeoutDeadObject();
    return result.second;
}

bool ProcessSkeleton::DetachDeadObject(IRemoteObject *object)
{
    bool ret = false;
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    std::unique_lock<std::shared_mutex> lockGuard(deadObjectMutex_);
    auto it = deadObjectRecord_.find(object);
    if (it != deadObjectRecord_.end()) {
        ZLOGD(LOG_LABEL, "erase %{public}zu handle:%{public}d desc:%{public}s", reinterpret_cast<uintptr_t>(object),
            it->second.handle, Str16ToStr8(it->second.desc).c_str());
        deadObjectRecord_.erase(it);
        ret = true;
    }
    DetachTimeoutDeadObject();
    return ret;
}

bool ProcessSkeleton::IsDeadObject(IRemoteObject *object)
{
    CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_, false);
    std::shared_lock<std::shared_mutex> lockGuard(deadObjectMutex_);
    auto it = deadObjectRecord_.find(object);
    if (it != deadObjectRecord_.end()) {
        ZLOGE(LOG_LABEL, "%{public}zu handle:%{public}d desc:%{public}s is deaded at time:%{public}" PRIu64,
            reinterpret_cast<uintptr_t>(object), it->second.handle, Str16ToStr8(it->second.desc).c_str(),
            it->second.deadTime);
        uint64_t curTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        auto &info = it->second;
        info.agingTime = curTime;
        return true;
    }
    return false;
}

void ProcessSkeleton::DetachTimeoutDeadObject()
{
    CHECK_INSTANCE_EXIT(exitFlag_);
    // don't lock in the function.
    uint64_t curTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if (curTime - deadObjectClearTime_ < DEAD_OBJECT_CHECK_INTERVAL) {
        return;
    }
    deadObjectClearTime_ = curTime;
    for (auto it = deadObjectRecord_.begin(); it != deadObjectRecord_.end();) {
        if (curTime - it->second.agingTime >= DEAD_OBJECT_TIMEOUT) {
            ZLOGD(LOG_LABEL, "erase %{public}zu handle:%{public}d desc:%{public}s time:%{public}" PRIu64,
                reinterpret_cast<uintptr_t>(it->first), it->second.handle, Str16ToStr8(it->second.desc).c_str(),
                it->second.deadTime);
            it = deadObjectRecord_.erase(it);
            continue;
        }
        ++it;
    }
}

bool ProcessSkeleton::GetThreadStopFlag()
{
    return stopThreadFlag_.load();
}

void ProcessSkeleton::IncreaseThreadCount()
{
    std::unique_lock<std::mutex> lockGuard(threadCountMutex_);
    runningChildThreadNum_.fetch_add(1);
}

void ProcessSkeleton::DecreaseThreadCount()
{
    std::unique_lock<std::mutex> lockGuard(threadCountMutex_);
    if (runningChildThreadNum_.load() > 0) {
        runningChildThreadNum_.fetch_sub(1);

        if (runningChildThreadNum_.load() == 0) {
            threadCountCon_.notify_one();
        }
    }
}

void ProcessSkeleton::NotifyChildThreadStop()
{
    // set child thread exit flag
    stopThreadFlag_.store(true);
    // after closeing fd, child threads will be not block in the 'WriteBinder' function
    BinderConnector *connector = BinderConnector::GetInstance();
    if (connector != nullptr) {
        connector->CloseDriverFd();
    }
    ZLOGI(LOG_LABEL, "start waiting for child thread to exit, child thread num:%{public}zu",
        runningChildThreadNum_.load());
    std::unique_lock<std::mutex> lockGuard(threadCountMutex_);
    threadCountCon_.wait_for(lockGuard,
        std::chrono::seconds(MAIN_THREAD_MAX_WAIT_TIME),
        [&threadNum = this->runningChildThreadNum_] { return threadNum.load() == 0; });
    if (runningChildThreadNum_.load() != 0) {
        ZLOGI(LOG_LABEL, "wait timeout, %{public}zu child threads not exiting", runningChildThreadNum_.load());
        return;
    }
    ZLOGI(LOG_LABEL, "wait finished, all child thread have exited");
}
} // namespace OHOS