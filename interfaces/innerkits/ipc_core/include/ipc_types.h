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

#ifndef OHOS_IPC_IPC_TYPES_H
#define OHOS_IPC_IPC_TYPES_H

#include <sys/types.h>
#include <errno.h>
#include "errors.h"

namespace OHOS {
#define ZIPC_PACK_CHARS(c1, c2, c3, c4) ((((c1) << 24)) | (((c2) << 16)) | (((c3) << 8)) | (c4))
#define SOFTBUS_SUBSYSTEM 203
#define SOFTBUS_IPC_ERRNO(module) (-((SOFTBUS_SUBSYSTEM << 21) | ((module) << 16) | (0xFFFF)))
constexpr int REGISTRY_HANDLE = 0;

enum SoftBusSubModule {
    IPCRPC_SUB_MODULE_CODE = 6,
};

/**
 * @brief Enumerates the commands and identifiers used in communication.
 * @since 9
 */
enum {
    FIRST_CALL_TRANSACTION = 0x00000001,
    LAST_CALL_TRANSACTION = 0x00ffffff,
    PING_TRANSACTION = ZIPC_PACK_CHARS('_', 'P', 'N', 'G'),
    DUMP_TRANSACTION = ZIPC_PACK_CHARS('_', 'D', 'M', 'P'),
    SHELL_COMMAND_TRANSACTION = ZIPC_PACK_CHARS('_', 'C', 'M', 'D'),
    INTERFACE_TRANSACTION = ZIPC_PACK_CHARS('_', 'N', 'T', 'F'),
    SYSPROPS_TRANSACTION = ZIPC_PACK_CHARS('_', 'S', 'P', 'R'),
    SYNCHRONIZE_REFERENCE = ZIPC_PACK_CHARS('_', 'S', 'Y', 'C'),
    INVOKE_LISTEN_THREAD = ZIPC_PACK_CHARS('_', 'I', 'L', 'T'),
    GET_PID_UID = ZIPC_PACK_CHARS('_', 'G', 'P', 'U'),
    GET_PROTO_INFO = ZIPC_PACK_CHARS('_', 'G', 'R', 'I'),
    GET_SESSION_NAME = ZIPC_PACK_CHARS('_', 'G', 'S', 'N'),
    GET_SESSION_NAME_PID_UID = ZIPC_PACK_CHARS('_', 'G', 'S', 'P'),
    GET_GRANTED_SESSION_NAME = ZIPC_PACK_CHARS('_', 'G', 'G', 'S'),
    DBINDER_OBITUARY_TRANSACTION = ZIPC_PACK_CHARS('_', 'D', 'O', 'T'),
    DBINDER_INCREFS_TRANSACTION = ZIPC_PACK_CHARS('_', 'D', 'I', 'T'),
    DBINDER_DECREFS_TRANSACTION = ZIPC_PACK_CHARS('_', 'D', 'D', 'T'),
    DBINDER_ADD_COMMAUTH = ZIPC_PACK_CHARS('_', 'D', 'A', 'C'),
    TRANS_SYNC = 0,
    TRANS_ASYNC = 1,
};

/**
 * @brief Enumerates the error types of communication.
 * @since 9
 */
enum {
    NO_ERROR = 0,
    TRANSACTION_ERR,
    FLATTEN_ERR = 3,
    UNKNOWN_TRANSACTION = 4,
    INVALID_DATA = 5,
    OBJECT_NULL = 7,
    INVALID_OPERATION = 8,
    DEAD_OBJECT = -EPIPE,
    UNKNOWN_ERROR,
};

constexpr int MIN_TRANSACTION_ID = 0x1;
constexpr int MAX_TRANSACTION_ID = 0x00ffffff;
constexpr int INVALID_FD = -1;

constexpr int32_t RPC_QOS_MIN_BW = 128 * 1024;
constexpr int32_t RPC_QOS_MAX_LATENCY = 10000;
constexpr int32_t RPC_QOS_MIN_LATENCY = 2000;
constexpr int32_t SOCKET_ID_INVALID = -1;

/**
 * @brief Enumerate the correct error code returned.
 * @since 9
 */
enum {
    ERR_NONE = 0,
    ERR_TRANSACTION_FAILED = 1,
    ERR_UNKNOWN_OBJECT = 2,
    ERR_FLATTEN_OBJECT = 3,
    ERR_UNKNOWN_TRANSACTION = 4,
    ERR_INVALID_DATA = 5,
    ERR_NULL_OBJECT = 7,
    ERR_UNKNOWN_REASON,
    ERR_INVALID_REPLY,
    ERR_INVALID_STATE,
    IPC_SKELETON_ERR = 100,
    IPC_SKELETON_NULL_OBJECT_ERR,
    IPC_PROXY_ERR = 200,
    IPC_PROXY_DEAD_OBJECT_ERR,
    IPC_PROXY_NULL_INVOKER_ERR,
    IPC_PROXY_TRANSACTION_ERR,
    IPC_PROXY_INVALID_CODE_ERR,
    IPC_STUB_ERR = 300,
    IPC_STUB_WRITE_PARCEL_ERR,
    IPC_STUB_INVOKE_THREAD_ERR,
    IPC_STUB_INVALID_DATA_ERR,
    IPC_STUB_CURRENT_NULL_ERR,
    IPC_STUB_UNKNOW_TRANS_ERR,
    IPC_STUB_CREATE_BUS_SERVER_ERR,
    IPC_INVOKER_ERR = 400,
    IPC_INVOKER_WRITE_TRANS_ERR,
    IPC_INVOKER_TRANSLATE_ERR,
    IPC_INVOKER_CONNECT_ERR,
    IPC_INVOKER_ON_TRANSACT_ERR,
    IPC_INVOKER_INVALID_DATA_ERR,
    IPC_INVOKER_INVALID_REPLY_ERR,
    RPC_BASE_INVOKER_ERR = 500,
    RPC_BASE_INVOKER_INVALID_REPLY_ERR,
    RPC_BASE_INVOKER_WAIT_REPLY_ERR,
    RPC_BASE_INVOKER_CURRENT_NULL_ERR,
    RPC_BASE_INVOKER_INVALID_DATA_ERR,
    RPC_BASE_INVOKER_WRITE_TRANS_ERR,
    RPC_BASE_INVOKER_SEND_REPLY_ERR,
    RPC_DATABUS_INVOKER_ERR = 600,
    RPC_DATABUS_INVOKER_INVALID_DATA_ERR,
    RPC_DATABUS_INVOKER_CLOSED_PEER_ERR,
    RPC_DATABUS_INVOKER_INVALID_STUB_INDEX,
    DBINDER_SERVICE_ERR = 700,
    DBINDER_SERVICE_INVALID_DATA_ERR,
    DBINDER_SERVICE_NOTICE_DIE_ERR,
    DBINDER_SERVICE_PROCESS_PROTO_ERR,
    DBINDER_SERVICE_UNKNOW_TRANS_ERR,
    DBINDER_SERVICE_ADD_DEATH_ERR,
    DBINDER_SERVICE_REMOVE_DEATH_ERR,
    DBINDER_SERVICE_WRONG_SESSION,
    SESSION_WRONG_FD_ERR = 800,
    SESSION_INVOKER_NULL_ERR,
    SESSION_UNAUTHENTICATED_ERR,
    SESSION_UNOPEN_ERR = -1,
    DBINDER_CALLBACK_ERR = 900,
    DBINDER_CALLBACK_ADD_DEATH_ERR,
    DBINDER_CALLBACK_RMV_DEATH_ERR,
    DBINDER_CALLBACK_READ_OBJECT_ERR,
    BINDER_CALLBACK_AUTHCOMM_ERR,
    BINDER_CALLBACK_STUBINDEX_ERR
};

/**
 * @brief Enumerate the radar event error code report.
 */
enum {
    /* IPC errno begin: -((203 << 21) | (6 << 16) | 0xFFFF) */
    RADAR_IPC_ERROR_CODE = SOFTBUS_IPC_ERRNO(IPCRPC_SUB_MODULE_CODE),
    RADAR_GET_SESSION_FAIL,
    RADAR_SEQ_INVALID,
    RADAR_SEND_RAW_DATA_FAIL,
    RADAR_SEND_NORMAL_DATA_FAIL,
    RADAR_MOVE_TRANS_DATA_TO_BUFFER_FAIL,
    RADAR_NO_NEED_REPLY,
    RADAR_RECEIVE_BUFFER_NULL,
    RADAR_BUFFER_NULL,
    RADAR_CREATE_RECV_ALLOCATOR_FAIL,
    RADAR_SET_ALLOCATOR_FAIL,
    RADAR_TRANSLATE_OBJECT_FAIL,
    RADAR_IPC_PROCESS_SKELETON_NULL,
    RADAR_SEND_ALLOCATOR_FAIL,
    RADAR_CHECK_AND_SET_CALLER_FAIL,
    RADAR_STUB_INVALID,
    RADAR_TRANSLATE_FAIL,
    RADAR_SEQ_MESSAGE_NULL,
    RADAR_SEQ_MESSAGE_BUFFER_NULL,
    RADAR_ERR_MEMCPY_DATA,
    RADAR_ERR_INVALID_DATA,
    RADAR_SEND_INVALID_HANDLE,
    RADAR_SEND_DISTRIBUTED_PROXY,
    RADAR_GET_SESSION_NAME_FAIL,
    RADAR_GET_BUS_SESSION_FAIL,
    RADAR_WRITE_TO_PARCEL_FAIL,
    RADAR_START_LISTEN_FAIL,
    RADAR_NEW_SERVER_SESSION_FAIL,
    RADAR_GET_REMOTE_LISTENER_FAIL,
    RADAR_SEND_DATA_TO_REMOTE_FAIL,
    RADAR_SEND_ENTRY_TO_REMOTE_FAIL,
    RADAR_ATTACH_THREADLOCK_FAIL,
    RADAR_WAIT_REPLY_TIMEOUT,
    RADAR_QUERY_REPLY_SESSION_FAIL,
    RADAR_ADD_DEATH_RECIPIENT_FAIL,
    RADAR_SEND_DATA_REPLAY_FAIL,
    RADAR_CALL_SYSTEM_ABILITY_FAIL,
    RADAR_GRT_SESSION_NAME_FAIL,
    RADAR_GET_SOFTBUS_SERVICE_FAIL,
    RADAR_GRANT_PERMISSION_FAIL,
    RADAR_GET_BUS_NAME_FAIL,
    RADAR_WRITE_PARCEL_FAIL,
    RADAR_INVOKE_STUB_THREAD_FAIL,
    RADAR_SESSION_NAME_INVALID,
    RADAR_IPC_NEW_SESSION_FAIL,
    RADAR_GET_SERVICE_NULL,
    RADAR_QUERY_SESSION_FAIL,
    RADAR_GET_UID_OR_PID_FAIL,
    RADAR_CREATE_BUS_NAME_FAIL,
    RADAR_TYPE_INVALID,
    RADAR_RECEIVED_PROXY_NULL,
    RADAR_ATTACH_DEATH_RECIPIENT_FAIL,
    RADAR_ATTACH_CALLBACK_PROXY_FAIL,
    RADAR_DETACH_DEATH_RECIPIENT_FAIL,
    RADAR_DETACH_CALLBACK_PROXY_FAIL,
    RADAR_GET_SA_OBJECT_NULL,
    RADAR_RPC_DATABUS_INVOKER_ERR,
    RADAR_BINDER_CALLBACK_AUTHCOMM_ERR,
    RADAR_CREATE_SOFTBUS_SERVER_FAIL,
    RADAR_OPEN_SESSION_FAIL,
    RADAR_SEND_BYTES_FAIL,
    RADAR_GET_PEER_SESSION_FAIL,
    RADAR_SERVICE_NO_START,
    RADAR_CREATE_BUFFER_FAIL,
    RADAR_DETACH_SESSION_FAIL,
    RADAR_ATTACH_SESSION_FAIL
};
} // namespace OHOS
#endif // OHOS_IPC_IPC_TYPES_H
