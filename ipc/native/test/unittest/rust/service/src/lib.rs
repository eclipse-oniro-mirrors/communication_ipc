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

//! This create implement the IPC proxy and stub for "test.ipc.ITestService"

extern crate ipc_rust;

mod access_token;

use ipc_rust::{
    IRemoteBroker, IRemoteObj, RemoteStub, Result,
    RemoteObj, define_remote_object, FIRST_CALL_TRANSACTION
};
use ipc_rust::{
    MsgParcel, BorrowedMsgParcel, FileDesc, InterfaceToken, String16,
};
pub use access_token::init_access_token;

/// Reverse a i32 value, for example reverse 2019 to 9102
pub fn reverse(mut value: i32) -> i32 {
    let mut result = 0;
    let decimal = 10;

    while value != 0 {
        result = result * decimal + value % decimal;
        value /= decimal;
    }
    result
}

/// SA ID for "test.ipc.ITestService"
pub const IPC_TEST_SERVICE_ID: i32 = 1118;

/// Function code of ITestService
pub enum ITestCode {
    /// Sync transaction code
    CodeSyncTransaction = FIRST_CALL_TRANSACTION,
    /// Async transaction code
    CodeAsyncTransaction,
    /// Ping transaction code
    CodePingService,
    /// Get FooService IPC object transaction code
    CodeGetFooService,
    /// Transaction file descriptor code
    CodeTransactFd,
    /// Transaction string code
    CodeTransactString,
    /// Transaction Interface token code
    CodeInterfaceToekn,
    /// Transaction calling infomation code
    CodeCallingInfo,
}

/// Function between proxy and stub of ITestService 
pub trait ITest: IRemoteBroker {
    /// Test sync transaction
    fn test_sync_transaction(&self, value: i32, delay_time: i32) -> Result<i32>;
    /// Test async transaction
    fn test_async_transaction(&self, value: i32, delay_time: i32) -> Result<()>;
    /// Test ping service transaction
    fn test_ping_service(&self, service_name: &String16) -> Result<()>;
    /// Test file descriptor transaction
    fn test_transact_fd(&self) -> Result<FileDesc>;
    /// Test string transaction
    fn test_transact_string(&self, value: &str) -> Result<i32>;
    /// Test get foo service IPC object transaction
    fn test_get_foo_service(&self) -> Result<RemoteObj>;
    /// Test interface token transaction
    fn echo_interface_token(&self, token: &InterfaceToken) -> Result<InterfaceToken>;
    /// Test calling infomation transaction
    fn echo_calling_info(&self) -> Result<(u64, u64, u64, u64)>;
}

fn on_itest_remote_request(stub: &dyn ITest, code: u32, data: &BorrowedMsgParcel,
    reply: &mut BorrowedMsgParcel) -> Result<()> {
    match code {
        1 => {
            let value: i32 = data.read().expect("should a value");
            let delay_time: i32 = data.read().expect("should a delay time");
            let ret = stub.test_sync_transaction(value, delay_time)?;
            reply.write(&ret)?;
            Ok(())
        }
        2 => {
            let value: i32 = data.read().expect("should a value");
            let delay_time: i32 = data.read().expect("should a delay time");
            stub.test_async_transaction(value, delay_time)?;
            Ok(())
        }
        3 => {
            let service_name: String16 = data.read().expect("should a service name");
            stub.test_ping_service(&service_name)?;
            Ok(())
        }
        4 => {
            let service = stub.test_get_foo_service()?;
            reply.write(&service)?;
            Ok(())
        }
        5 => {
            let fd = stub.test_transact_fd()?;
            reply.write(&fd).expect("should write fd success");
            Ok(())
        }
        6 => {
            let value: String = data.read()?;
            let len = stub.test_transact_string(&value)?;
            reply.write(&len)?;
            Ok(())
        }
        7 => {
            let token: InterfaceToken = data.read().expect("should have a interface token");
            let value = stub.echo_interface_token(&token).expect("service deal echo token failed");
            reply.write(&value).expect("write echo token result failed");
            Ok(())
        }
        8 => {
            let (token_id, first_token_id, pid, uid) = stub.echo_calling_info()?;
            reply.write(&token_id).expect("write token id failed");
            reply.write(&first_token_id).expect("write first token id failed");
            reply.write(&pid).expect("write pid failed");
            reply.write(&uid).expect("write uid failed");
            Ok(())
        }
        _ => Err(-1)
    }
}

define_remote_object!(
    ITest["test.ipc.ITestService"] {
        stub: TestStub(on_itest_remote_request),
        proxy: TestProxy,
    }
);

// Make RemoteStub<TestStub> object can call ITest function directly.
impl ITest for RemoteStub<TestStub> {
    fn test_sync_transaction(&self, value: i32, delay_time: i32) -> Result<i32> {
        self.0.test_sync_transaction(value, delay_time)
    }

    fn test_async_transaction(&self, value: i32, delay_time: i32) -> Result<()> {
        self.0.test_async_transaction(value, delay_time)
    }

    fn test_ping_service(&self, service_name: &String16) -> Result<()> {
        self.0.test_ping_service(service_name)
    }

    fn test_transact_fd(&self) -> Result<FileDesc> {
        self.0.test_transact_fd()
    }

    fn test_transact_string(&self, value: &str) -> Result<i32> {
        self.0.test_transact_string(value)
    }

    fn test_get_foo_service(&self) -> Result<RemoteObj> {
        self.0.test_get_foo_service()
    }

    fn echo_interface_token(&self, token: &InterfaceToken) -> Result<InterfaceToken> {
        self.0.echo_interface_token(token)
    }

    fn echo_calling_info(&self) -> Result<(u64, u64, u64, u64)> {
        self.0.echo_calling_info()
    }
}

impl ITest for TestProxy {
    fn test_sync_transaction(&self, value: i32, delay_time: i32) -> Result<i32> {
        let mut data = MsgParcel::new().expect("MsgParcel should success");
        data.write(&value)?;
        data.write(&delay_time)?;
        let reply = self.remote.send_request(ITestCode::CodeSyncTransaction as u32,
            &data, false)?;
        let ret: i32 = reply.read().expect("need reply i32");
        Ok(ret)
    }

    fn test_async_transaction(&self, value: i32, delay_time: i32) -> Result<()> {
        let mut data = MsgParcel::new().expect("MsgParcel should success");
        data.write(&value)?;
        data.write(&delay_time)?;
        let _reply = self.remote.send_request(ITestCode::CodeAsyncTransaction as u32,
            &data, true)?;
        Ok(())
    }

    fn test_ping_service(&self, service_name: &String16) -> Result<()> {
        let mut data = MsgParcel::new().expect("MsgParcel should success");
        data.write(service_name)?;
        let _reply = self.remote.send_request(ITestCode::CodePingService as u32,
            &data, false)?;
        Ok(())
    }

    fn test_transact_fd(&self) -> Result<FileDesc> {
        let data = MsgParcel::new().expect("MsgParcel should success");
        let reply = self.remote.send_request(ITestCode::CodeTransactFd as u32,
            &data, false)?;
        let fd: FileDesc = reply.read()?;
        Ok(fd)
    }

    fn test_transact_string(&self, value: &str) -> Result<i32> {
        let mut data = MsgParcel::new().expect("MsgParcel should success");
        data.write(value).expect("should write string success");
        let reply = self.remote.send_request(ITestCode::CodeTransactString as u32,
            &data, false)?;
        let len: i32 = reply.read()?;
        Ok(len)
    }

    fn test_get_foo_service(&self) -> Result<RemoteObj> {
        let data = MsgParcel::new().expect("MsgParcel should success");
        let reply = self.remote.send_request(ITestCode::CodeGetFooService as u32,
            &data, false)?;
        let service: RemoteObj = reply.read()?;
        Ok(service)
    }

    fn echo_interface_token(&self, token: &InterfaceToken) -> Result<InterfaceToken> {
        let mut data = MsgParcel::new().expect("MsgParcel should success");
        data.write(token).expect("write token should success");
        let reply = self.remote.send_request(ITestCode::CodeInterfaceToekn as u32,
            &data, false)?;
        let echo_value: InterfaceToken = reply.read().expect("need reply token");
        Ok(echo_value)
    }

    fn echo_calling_info(&self) -> Result<(u64, u64, u64, u64)> {
        let data = MsgParcel::new().expect("MsgParcel should success");
        let reply = self.remote.send_request(ITestCode::CodeCallingInfo as u32,
            &data, false)?;
        let token_id: u64 = reply.read().expect("need reply calling token id");
        let first_token_id: u64 = reply.read().expect("need reply first calling token id");
        let pid: u64 = reply.read().expect("need reply calling pid");
        let uid: u64 = reply.read().expect("need reply calling uid");
        Ok((token_id, first_token_id, pid, uid))
    }
}

/// Interface trait for FooService
pub trait IFoo: IRemoteBroker {
}

fn on_foo_remote_request(_stub: &dyn IFoo, _code: u32, _data: &BorrowedMsgParcel,
    _reply: &mut BorrowedMsgParcel) -> Result<()> {
    Ok(())
}

impl IFoo for RemoteStub<FooStub> {
}

impl IFoo for FooProxy {
}

define_remote_object!(
    IFoo["ohos.ipc.test.foo"] {
        stub: FooStub(on_foo_remote_request),
        proxy: FooProxy,
    }
);