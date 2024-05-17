/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NAPI_IPC_OHOS_NAPI_RPC_COMMON_H
#define NAPI_IPC_OHOS_NAPI_RPC_COMMON_H

namespace OHOS {
#define CHECK_WRITE_CAPACITY(env, lenToWrite, napiParcel)                                                   \
    do {                                                                                                    \
        size_t cap =  (napiParcel)->maxCapacityToWrite_ - (napiParcel)->nativeParcel_->GetWritePosition();  \
        if (cap < (lenToWrite)) {                                                                           \
            ZLOGI(LOG_LABEL, "No enough capacity to write");                                                \
            napi_throw_range_error(env, nullptr, "No enough capacity to write");                            \
        }                                                                                                   \
    } while (0)

#define REWIND_IF_WRITE_CHECK_FAIL(env, lenToWrite, pos, napiParcel)                                        \
    do {                                                                                                    \
        size_t cap = (napiParcel)->maxCapacityToWrite_ - (napiParcel)->nativeParcel_->GetWritePosition();   \
        if (cap < (lenToWrite)) {                                                                           \
            ZLOGI(LOG_LABEL, "No enough capacity to write");                                                \
            (napiParcel)->nativeParcel_->RewindWrite(pos);                                                  \
            napi_throw_range_error(env, nullptr, "No enough capacity to write");                            \
        }                                                                                                   \
    } while (0)

#define CHECK_READ_LENGTH(env, arrayLength, typeSize, napiParcel)                                                    \
    do {                                                                                                             \
        size_t remainSize = (napiParcel)->nativeParcel_->GetDataSize() -                                             \
            (napiParcel)->nativeParcel_->GetReadPosition();                                                          \
        if (((arrayLength) < 0) || ((arrayLength) > remainSize) || (((arrayLength) * (typeSize)) > remainSize)) {    \
            ZLOGI(LOG_LABEL, "No enough data to read");                                                              \
            napi_throw_range_error(env, nullptr, "No enough data to read");                                          \
        }                                                                                                            \
    } while (0)

static constexpr size_t MAX_CAPACITY_TO_WRITE = 200 * 1024;
static constexpr size_t MAX_BYTES_LENGTH = 40960;
static constexpr size_t BYTE_SIZE_8 = 1;
static constexpr size_t BYTE_SIZE_32 = 4;
static constexpr size_t BYTE_SIZE_64 = 8;
static constexpr size_t ARGV_INDEX_0 = 0;
static constexpr size_t ARGV_INDEX_1 = 1;
static constexpr size_t ARGV_LENGTH_1 = 1;
static constexpr size_t ARGV_LENGTH_2 = 2;
static constexpr size_t REQUIRED_ARGS_COUNT_1 = 1;  // "requires 1 parameter"
} // namespace OHOS
#endif //  NAPI_IPC_OHOS_NAPI_RPC_COMMON_H