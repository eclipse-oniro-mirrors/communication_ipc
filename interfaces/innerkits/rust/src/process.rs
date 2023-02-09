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

use crate::{
    ipc_binding, MsgParcel, RemoteObj, IRemoteObj, InterfaceToken, String16,
    Result,
};
use std::ffi::{CString, c_char};
use hilog_rust::{info, hilog, HiLogLabel, LogType};

const LOG_LABEL: HiLogLabel = HiLogLabel {
    log_type: LogType::LogCore,
    domain: 0xd001510,
    tag: "RustProcess"
};

/// Get proxy object of samgr
pub fn get_context_object() -> Option<RemoteObj>
{
    unsafe {
        let samgr = ipc_binding::GetContextManager();
        RemoteObj::from_raw(samgr)
    }
}

/// Add a service to samgr
pub fn add_service(service: &RemoteObj, said: i32) -> Result<()>
{
    let samgr = get_context_object().expect("samgr is not null");
    let mut data = MsgParcel::new().expect("MsgParcel is not null");
    data.write(&InterfaceToken::new("ohos.samgr.accessToken"))?;
    data.write(&said)?;
    data.write(service)?;
    data.write(&false)?;
    data.write(&0)?;
    data.write(&String16::new(""))?;
    data.write(&String16::new(""))?;
    let reply = samgr.send_request(3, &data, false)?;
    let reply_value: i32 = reply.read()?;
    info!(LOG_LABEL, "register service result: {}", reply_value);
    if reply_value == 0 { Ok(())} else { Err(reply_value) }
}

/// Get a service proxy from samgr
pub fn get_service(said: i32) -> Result<RemoteObj>
{
    let samgr = get_context_object().expect("samgr is not null");
    let mut data = MsgParcel::new().expect("MsgParcel is not null");
    data.write(&InterfaceToken::new("ohos.samgr.accessToken"))?;
    data.write(&said)?;
    let reply = samgr.send_request(2, &data, false)?;
    let remote: RemoteObj = reply.read()?;
    info!(LOG_LABEL, "get service success");
    Ok(remote)
}

/// Make current thread join to the IPC/RPC work thread pool
#[inline]
pub fn join_work_thread()
{
    unsafe {
        ipc_binding::JoinWorkThread();
    }
}

/// Exit current thread from IPC/RPC work thread pool
#[inline]
pub fn stop_work_thread()
{
    unsafe {
        ipc_binding::StopWorkThread()
    }
}

/// Get calling token ID of caller
#[inline]
pub fn get_calling_token_id() -> u64
{
    unsafe {
        ipc_binding::GetCallingTokenId()
    }
}

/// Get first calling token ID of caller
#[inline]
pub fn get_first_token_id() -> u64
{
    unsafe {
        ipc_binding::GetFirstToekenId()
    }
}

/// Get self token id of current process
#[inline]
pub fn get_self_token_id() -> u64
{
    unsafe {
        ipc_binding::GetSelfToekenId()
    }
}

/// Get calling process id of caller
#[inline]
pub fn get_calling_pid() -> u64
{
    unsafe {
        ipc_binding::GetCallingPid()
    }
}

/// Get calling user id of caller
#[inline]
pub fn get_calling_uid() -> u64
{
    unsafe {
        ipc_binding::GetCallingUid()
    }
}