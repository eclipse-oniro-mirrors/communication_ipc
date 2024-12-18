/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "setipcproxylimit_fuzzer.h"
#include "ipc_skeleton.h"
#include "message_parcel.h"
#include "securec.h"

namespace OHOS {
void SetIPCProxyLimitFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }

    OHOS::MessageParcel parcel;
    parcel.WriteBuffer(data, size);
    int ipcProxyLimitNum = parcel.ReadInt32();

    IPCDfx::IPCProxyLimitCallback callback = [] (uint64_t num) {
        (void)num;
    }

    IPCDfx::SetIPCProxyLimit(ipcProxyLimitNum, callback);
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::SetIPCProxyLimitFuzzTest(data, size);
    return 0;
}