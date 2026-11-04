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

#ifndef OHOS_NAPI_ERROR_H
#define OHOS_NAPI_ERROR_H

#include<string>
#include<map>

#include "napi/native_node_api.h"

namespace OHOS {
enum errorDesc {
    CHECK_PARAM_ERROR,
    OS_MMAP_ERROR,
    OS_IOCTL_ERROR,
    WRITE_TO_ASHMEM_ERROR,
    READ_FROM_ASHMEM_ERROR,
    ONLY_PROXY_OBJECT_PERMITTED_ERROR,
    ONLY_REMOTE_OBJECT_PERMITTED_ERROR,
    COMMUNICATION_ERROR,
    PROXY_OR_REMOTE_OBJECT_INVALID_ERROR,
    WRITE_DATA_TO_MESSAGE_SEQUENCE_ERROR,
    READ_DATA_FROM_MESSAGE_SEQUENCE_ERROR,
    PARCEL_MEMORY_ALLOC_ERROR,
    CALL_JS_METHOD_ERROR,
    OS_DUP_ERROR
};

typedef struct errorInfo {
    int errorCode;
    std::string errorMsg;
} errorInfo;

class NapiError {
public:
    NapiError() {};
    NapiError(int32_t errorCode) : errorCode_(errorCode) {};
    napi_value GetError(napi_env& env) const;
    napi_value ThrowError(napi_env& env, int32_t code = -1);
    napi_value ThrowParamError(napi_env& env, const std::string& msg);
    inline void Error(int32_t errorCode)
    {
        errorCode_ = (errorCode != -1) ? errorCode : errorCode_;
    };
    inline bool IsError() const
    {
        return errorCode_ != -1;
    };

    static napi_value NAPIRpcErrorEnumExport(napi_env env, napi_value exports);
    static std::string NapiTypeToString(napi_valuetype type)
    {
        switch (type) {
            case napi_undefined: return "undefined";
            case napi_null: return "null";
            case napi_boolean: return "boolean";
            case napi_number: return "number";
            case napi_string: return "string";
            case napi_symbol: return "symbol";
            case napi_object: return "object";
            case napi_function: return "function";
            case napi_bigint: return "bigint";
            default: return "unknown";
        }
    }
private:
    int32_t errorCode_{-1};
    static std::map<int32_t, errorInfo> napiErrMap_;
}; // NapiError
} // namespace OHOS
#endif // OHOS_NAPI_ERROR_H
