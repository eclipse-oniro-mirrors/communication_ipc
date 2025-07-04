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

#ifndef OHOS_IPC_SYS_BINDER_H
#define OHOS_IPC_SYS_BINDER_H

#include <sys/types.h>

#ifndef _UAPI_LINUX_BINDER_H
#define _UAPI_LINUX_BINDER_H

#include <linux/types.h>
#include <linux/ioctl.h>

#ifndef B_PACK_CHARS
#define B_PACK_CHARS(c1, c2, c3, c4) ((((c1) << 24)) | (((c2) << 16)) | (((c3) << 8)) | (c4))
#endif

#define B_TYPE_LARGE 0x85
enum {
    BINDER_TYPE_BINDER = B_PACK_CHARS('s', 'b', '*', B_TYPE_LARGE),
    BINDER_TYPE_WEAK_BINDER = B_PACK_CHARS('w', 'b', '*', B_TYPE_LARGE),
    BINDER_TYPE_HANDLE = B_PACK_CHARS('s', 'h', '*', B_TYPE_LARGE),
    BINDER_TYPE_WEAK_HANDLE = B_PACK_CHARS('w', 'h', '*', B_TYPE_LARGE),
    BINDER_TYPE_FD = B_PACK_CHARS('f', 'd', '*', B_TYPE_LARGE),
    BINDER_TYPE_FDA = B_PACK_CHARS('f', 'd', 'a', B_TYPE_LARGE),
    BINDER_TYPE_PTR = B_PACK_CHARS('p', 't', '*', B_TYPE_LARGE),
};
enum {
    FLAT_BINDER_FLAG_PRIORITY_MASK = 0xff,
    FLAT_BINDER_FLAG_ACCEPTS_FDS = 0x100,
    FLAT_BINDER_FLAG_TXN_SECURITY_CTX = 0x1000,
};
#ifdef BINDER_IPC_32BIT
typedef __u32 binder_size_t;
typedef __u32 binder_uintptr_t;
#else
typedef __u64 binder_size_t;
typedef __u64 binder_uintptr_t;
#endif
struct binder_object_header {
    __u32 type;
};
struct flat_binder_object {
    struct binder_object_header hdr;
    __u32 flags;
    union {
        binder_uintptr_t binder;
        __u32 handle;
    };
    binder_uintptr_t cookie;
};
#ifndef __linux__
#define HMB_ERROR_INFO_LEN 64
struct hmb_detailed_err {
    uint32_t err_code;
    char err_str[HMB_ERROR_INFO_LEN];
};
#endif
struct binder_fd_object {
    struct binder_object_header hdr;
    __u32 pad_flags;
    union {
        binder_uintptr_t pad_binder;
        __u32 fd;
    };
    binder_uintptr_t cookie;
};
struct binder_buffer_object {
    struct binder_object_header hdr;
    __u32 flags;
    binder_uintptr_t buffer;
    binder_size_t length;
    binder_size_t parent;
    binder_size_t parent_offset;
};
enum {
    BINDER_BUFFER_FLAG_HAS_PARENT = 0x01,
    BINDER_BUFFER_FLAG_HAS_DBINDER = 0x01 << 31,
};
struct binder_fd_array_object {
    struct binder_object_header hdr;
    __u32 pad;
    binder_size_t num_fds;
    binder_size_t parent;
    binder_size_t parent_offset;
};
struct binder_write_read {
    binder_size_t write_size;
    binder_size_t write_consumed;
    binder_uintptr_t write_buffer;
    binder_size_t read_size;
    binder_size_t read_consumed;
    binder_uintptr_t read_buffer;
};
struct binder_version {
    __s32 protocol_version;
};
#ifdef BINDER_IPC_32BIT
#define BINDER_CURRENT_PROTOCOL_VERSION 7
#else
#define BINDER_CURRENT_PROTOCOL_VERSION 8
#endif
struct binder_node_debug_info {
    binder_uintptr_t ptr;
    binder_uintptr_t cookie;
    __u32 has_strong_ref;
    __u32 has_weak_ref;
};

struct binder_node_info_for_ref {
    __u32 handle;
    __u32 strong_count;
    __u32 weak_count;
    __u32 reserved1;
    __u32 reserved2;
    __u32 reserved3;
};

#define ACCESS_TOKENID_GET_TOKENID _IOR('A', 1, unsigned long long)
#define ACCESS_TOKENID_GET_FTOKENID _IOR('A', 3, unsigned long long)
#define ACCESS_TOKEN_FAETURE_MASK (1 << 0)
#define SENDER_INFO_FAETURE_MASK (1 << 2)
//      ACTV_BINDER_FEATURE_MASK  (1 << 1)

struct binder_feature_set {
    __u64 feature_set;
};

struct access_token {
    __u64 sender_tokenid;
    __u64 first_tokenid;
    __u64 reserved[2];
};

struct binder_sender_info {
    struct access_token tokens;
    __u64 sender_pid_nr;
};

#define BINDER_WRITE_READ _IOWR('b', 1, struct binder_write_read)
#define BINDER_SET_IDLE_TIMEOUT _IOW('b', 3, __s64)
#define BINDER_SET_MAX_THREADS _IOW('b', 5, __u32)
#define BINDER_SET_IDLE_PRIORITY _IOW('b', 6, __s32)
#define BINDER_SET_CONTEXT_MGR _IOW('b', 7, __s32)
#define BINDER_THREAD_EXIT _IOW('b', 8, __s32)
#define BINDER_VERSION _IOWR('b', 9, struct binder_version)
#define BINDER_GET_NODE_DEBUG_INFO _IOWR('b', 11, struct binder_node_debug_info)
#define BINDER_GET_NODE_INFO_FOR_REF _IOWR('b', 12, struct binder_node_info_for_ref)
#define BINDER_SET_CONTEXT_MGR_EXT _IOW('b', 13, struct flat_binder_object)
#define BINDER_FEATURE_SET _IOWR('b', 30, struct binder_feature_set)
#define BINDER_GET_ACCESS_TOKEN _IOWR('b', 31, struct access_token)
#define BINDER_GET_SENDER_INFO _IOWR('b', 32, struct binder_sender_info)
#define BINDER_THREAD_RECLAIM _IOW('b', 33, __s32)

enum transaction_flags {
    TF_ONE_WAY = 0x01,
    TF_ROOT_OBJECT = 0x04,
    TF_STATUS_CODE = 0x08,
    TF_ACCEPT_FDS = 0x10,
    TF_HITRACE = 0x80, // add flag for hitrace
};
struct binder_transaction_data {
    union {
        __u32 handle;
        binder_uintptr_t ptr;
    } target;
    binder_uintptr_t cookie;
    __u32 code;
    __u32 flags;
    pid_t sender_pid;
    uid_t sender_euid;
    binder_size_t data_size;
    binder_size_t offsets_size;
    union {
        struct {
            binder_uintptr_t buffer;
            binder_uintptr_t offsets;
        } ptr;
        __u8 buf[8];
    } data;
};

struct binder_transaction_data_secctx {
    struct binder_transaction_data transaction_data;
    binder_uintptr_t secctx;
};

struct binder_transaction_data_sg {
    struct binder_transaction_data transaction_data;
    binder_size_t buffers_size;
};

struct binder_ptr_cookie {
    binder_uintptr_t ptr;
    binder_uintptr_t cookie;
};
struct binder_handle_cookie {
    __u32 handle;
    binder_uintptr_t cookie;
} __attribute__((__packed__));
struct binder_pri_desc {
    __s32 priority;
    __u32 desc;
};
struct binder_pri_ptr_cookie {
    __s32 priority;
    binder_uintptr_t ptr;
    binder_uintptr_t cookie;
};

/* 31~30(2bit) rw, 29~16(14bit) size, 15~8(8bit) magic, 7~0(8bit) seq */
enum binder_driver_return_protocol {
    BR_ERROR = _IOR('r', 0, __s32),
    BR_OK = _IO('r', 1),
    BR_TRANSACTION_SEC_CTX = _IOR('r', 2, struct binder_transaction_data_secctx),
    BR_TRANSACTION = _IOR('r', 2, struct binder_transaction_data),
    BR_REPLY = _IOR('r', 3, struct binder_transaction_data),
    BR_DEAD_REPLY = _IO('r', 5),
    BR_TRANSACTION_COMPLETE = _IO('r', 6),
    BR_INCREFS = _IOR('r', 7, struct binder_ptr_cookie),
    BR_ACQUIRE = _IOR('r', 8, struct binder_ptr_cookie),
    BR_RELEASE = _IOR('r', 9, struct binder_ptr_cookie),
    BR_DECREFS = _IOR('r', 10, struct binder_ptr_cookie),
    BR_ATTEMPT_ACQUIRE = _IOR('r', 11, struct binder_pri_ptr_cookie),
    BR_NOOP = _IO('r', 12),
    BR_SPAWN_LOOPER = _IO('r', 13),
    BR_FINISHED = _IO('r', 14),
    BR_DEAD_BINDER = _IOR('r', 15, binder_uintptr_t),
    BR_CLEAR_DEATH_NOTIFICATION_DONE = _IOR('r', 16, binder_uintptr_t),
    BR_FAILED_REPLY = _IO('r', 17),
    BR_RELEASE_NODE = _IO('r', 18),
};
enum binder_driver_command_protocol {
    BC_TRANSACTION = _IOW('c', 0, struct binder_transaction_data),
    BC_REPLY = _IOW('c', 1, struct binder_transaction_data),
    BC_ACQUIRE_RESULT = _IOW('c', 2, __s32),
    BC_FREE_BUFFER = _IOW('c', 3, binder_uintptr_t),
    BC_INCREFS = _IOW('c', 4, __u32),
    BC_ACQUIRE = _IOW('c', 5, __u32),
    BC_RELEASE = _IOW('c', 6, __u32),
    BC_DECREFS = _IOW('c', 7, __u32),
    BC_INCREFS_DONE = _IOW('c', 8, struct binder_ptr_cookie),
    BC_ACQUIRE_DONE = _IOW('c', 9, struct binder_ptr_cookie),
    BC_ATTEMPT_ACQUIRE = _IOW('c', 10, struct binder_pri_desc),
    BC_REGISTER_LOOPER = _IO('c', 11),
    BC_ENTER_LOOPER = _IO('c', 12),
    BC_EXIT_LOOPER = _IO('c', 13),
    BC_REQUEST_DEATH_NOTIFICATION = _IOW('c', 14, struct binder_handle_cookie),
    BC_CLEAR_DEATH_NOTIFICATION = _IOW('c', 15, struct binder_handle_cookie),
    BC_DEAD_BINDER_DONE = _IOW('c', 16, binder_uintptr_t),
    BC_TRANSACTION_SG = _IOW('c', 17, struct binder_transaction_data_sg),
    BC_REPLY_SG = _IOW('c', 18, struct binder_transaction_data_sg),
    BC_SEND_RAWDATA = _IOW('c', 20, __u32),
    BC_TRANSLATION = _IOW('c', 21, struct flat_binder_object),
#ifndef __linux__
    HMB_GET_DETAILED_ERROR = _IOWR('b', 161, struct hmb_detailed_err),
#endif
};
#endif /* * _UAPI_LINUX_BINDER_H * */

static const uint32_t DBINDER_MAGICWORD = 0X4442494E;
static const uint32_t ENCRYPT_HEAD_LEN = 28;
static const uint32_t DEVICEID_LENGTH = 64;
static const uint32_t PID_LEN = 32;
static const uint32_t RPC_DEFAULT_VERSION_NUM = 1;
static const uint32_t SUPPORT_TOKENID_VERSION_NUM = 2;
static const uint32_t TOKENID_MAGIC = 0X544F4B49;
static const uint32_t SESSION_NAME_LENGTH = 64;
static const uint32_t DBINDER_DESC_LENGTH = 64;

enum {
    BINDER_TYPE_REMOTE_BINDER = B_PACK_CHARS('r', 'b', '*', B_TYPE_LARGE),
    BINDER_TYPE_REMOTE_HANDLE = B_PACK_CHARS('r', 'h', '*', B_TYPE_LARGE),
    ZBINDER_TYPE_REMOTE_NODE = B_PACK_CHARS('r', 'n', '*', B_TYPE_LARGE),
    ZBINDER_TYPE_NODE = B_PACK_CHARS('s', 'n', '*', B_TYPE_LARGE),
    BINDER_TYPE_FDR = B_PACK_CHARS('f', 'd', 'r', B_TYPE_LARGE),
    BINDER_TYPE_INVALID_HANDLE = B_PACK_CHARS('r', 'h', 'e', B_TYPE_LARGE),
    BINDER_TYPE_INVALID_BINDER = B_PACK_CHARS('r', 'b', 'e', B_TYPE_LARGE),
    BINDER_TYPE_INVALID_TYPE = B_PACK_CHARS('r', 't', 'e', B_TYPE_LARGE),
};

struct binder_ptr_count {
    binder_uintptr_t ptr;
    uint32_t count;
};

struct dbinder_transaction_data {
    __u32 sizeOfSelf;
    __u32 magic;
    __u32 version;
    int cmd;
    __u32 code;
    __u32 flags;
    __u64 cookie;
    __u64 seqNumber;
    binder_size_t buffer_size;
    binder_size_t offsets_size;
    binder_uintptr_t offsets;
    unsigned char buffer[];
};

struct dbinder_negotiation_data {
    __u32 proto;
    __u32 tokenid;
    __u64 stub_index;
    char target_name[SESSION_NAME_LENGTH + 1];
    char local_name[SESSION_NAME_LENGTH + 1];
    char target_device[DEVICEID_LENGTH + 1];
    char local_device[DEVICEID_LENGTH + 1];
    char16_t desc[DBINDER_DESC_LENGTH + 1];
    char reserved[3];
};
#endif // OHOS_IPC_SYS_BINDER_H
