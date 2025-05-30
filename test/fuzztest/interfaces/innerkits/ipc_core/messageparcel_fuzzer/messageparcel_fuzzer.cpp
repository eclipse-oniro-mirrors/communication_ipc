/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "messageparcel_fuzzer.h"
#include "iremote_object.h"
#include "message_parcel.h"
#include <iostream>

namespace OHOS {
void WriteRawDataFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }
    MessageParcel parcel;
    parcel.WriteRawData((const void *)data, size);
}

void WriteRemoteObjectFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }
    MessageParcel parcel;
    parcel.WriteBuffer(data, size);
    const sptr<IRemoteObject> object = parcel.ReadRemoteObject();
    parcel.WriteRemoteObject(object);
}

void WriteInterfaceTokenFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }
    MessageParcel parcel;
    parcel.WriteBuffer(data, size);
    size_t length = parcel.GetReadableBytes();
    if (length == 0) {
        return;
    }
    const char *bufData = reinterpret_cast<const char *>(parcel.ReadBuffer(length));
    if (bufData == nullptr) {
        return;
    }
    std::string tokenStr(bufData, length);
    std::u16string token(tokenStr.begin(), tokenStr.end());
    parcel.WriteInterfaceToken(token);
}

void WriteFileDescriptorFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }
    MessageParcel parcel;
    parcel.WriteBuffer(data, size);
    int fd = parcel.ReadInt32();
    parcel.WriteFileDescriptor(fd);
}

void ReadRawDataFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }
    MessageParcel parcel;
    parcel.WriteBuffer(data, size);
    size_t len = parcel.ReadUint64();
    parcel.ReadRawData(len);
}

void PrintBufferFuzzTest(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return;
    }
    MessageParcel parcel;
    parcel.WriteRawData((const void *)data, size);
    parcel.PrintBuffer(__FUNCTION__, __LINE__);
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::WriteRawDataFuzzTest(data, size);
    OHOS::WriteRemoteObjectFuzzTest(data, size);
    OHOS::WriteInterfaceTokenFuzzTest(data, size);
    OHOS::WriteFileDescriptorFuzzTest(data, size);
    OHOS::ReadRawDataFuzzTest(data, size);
    OHOS::PrintBufferFuzzTest(data, size);
    return 0;
}
