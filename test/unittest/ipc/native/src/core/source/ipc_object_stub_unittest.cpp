/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
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

#include <algorithm>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <pthread.h>

#include "dbinder_databus_invoker.h"
#include "dbinder_softbus_client.h"
#include "ipc_skeleton.h"
#include "ipc_object_proxy.h"
#include "ipc_object_stub.h"
#include "ipc_process_skeleton.h"
#include "ipc_thread_skeleton.h"
#include "mock_iremote_invoker.h"

using namespace testing;
using namespace testing::ext;
using namespace OHOS;
namespace OHOS {

namespace {
const std::string DEVICEID_TEST = "testRemoteDeviceId";
const std::string SESSION_NAME_TEST = "testSessionName";
constexpr uint32_t CODE_TEST = 0;
constexpr uint32_t REMOTEPID_TEST = 1;
constexpr uint32_t REMOTEUID_TEST = 1;
constexpr uint32_t STUBINDEX_TEST = 1;
constexpr uint32_t TOKEN_ID_TEST = 1;
constexpr uint32_t REMOTEFEATURE_TEST = 1;
constexpr uint64_t STUBINDEX_TEST2 = 0;
constexpr int UID_TEST = 123;
constexpr int PID_TEST = 456;
constexpr int SHELL_UID = 2000;
constexpr int HIDUMPER_SERVICE_UID = 1212;
constexpr int TEST_OBJECT_HANDLE = 16;
}

class IPCObjectStubTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void IPCObjectStubTest::SetUpTestCase()
{
}

void IPCObjectStubTest::TearDownTestCase()
{
}

void IPCObjectStubTest::SetUp()
{
}

void IPCObjectStubTest::TearDown()
{
}

class IpcObjectStubInterface {
public:
    IpcObjectStubInterface() {};
    virtual ~IpcObjectStubInterface() {};
    virtual bool ReadString16Vector(std::vector<std::u16string> *val) = 0;
    virtual int  ReadFileDescriptor() = 0;
    virtual bool WriteInt32(int32_t value) = 0;
    virtual bool WriteString16(const std::u16string &value) = 0;
    virtual bool WriteString(const std::string &value) = 0;
    virtual pid_t GetCallingUid() = 0;
    virtual pid_t GetCallingPid() = 0;
    virtual uint32_t GetCallingTokenID() = 0;
    virtual std::string GetCallingDeviceID() = 0;
    virtual bool IsLocalCalling() = 0;
    virtual sptr<IRemoteObject> GetSAMgrObject() = 0;
    virtual std::string GetGrantedSessionName() = 0;
    virtual uint32_t ReadUint32() = 0;
    virtual const std::string ReadString() = 0;
    virtual bool WriteUint64(uint64_t value) = 0;
    virtual bool WriteUint32(uint32_t value) = 0;
    virtual uint64_t ReadUint64() = 0;
    virtual uint64_t AddStubByIndex(IRemoteObject *stubObject) = 0;
    virtual bool CreateSoftbusServer(const std::string &name) = 0;
    virtual bool AttachOrUpdateAppAuthInfo(const AppAuthInfo &appAuthInfo) = 0;
    virtual bool DetachAppAuthInfo(const AppAuthInfo &appAuthInfo) = 0;
};

class IpcObjectStubInterfaceMock : public IpcObjectStubInterface {
public:
    IpcObjectStubInterfaceMock();
    ~IpcObjectStubInterfaceMock() override;
    MOCK_METHOD1(ReadString16Vector, bool(std::vector<std::u16string> *val));
    MOCK_METHOD0(ReadFileDescriptor, int());
    MOCK_METHOD1(WriteInt32, bool(int32_t value));
    MOCK_METHOD1(WriteString16, bool(const std::u16string &value));
    MOCK_METHOD1(WriteString, bool(const std::string &value));
    MOCK_METHOD0(GetCallingUid, pid_t());
    MOCK_METHOD0(GetCallingPid, pid_t());
    MOCK_METHOD0(GetCallingTokenID, uint32_t());
    MOCK_METHOD0(GetCallingDeviceID, std::string());
    MOCK_METHOD0(IsLocalCalling, bool());
    MOCK_METHOD0(GetSAMgrObject, sptr<IRemoteObject>());
    MOCK_METHOD0(GetGrantedSessionName, std::string());
    MOCK_METHOD0(ReadUint32, uint32_t());
    MOCK_METHOD0(ReadString, const std::string());
    MOCK_METHOD1(WriteUint64, bool(uint64_t value));
    MOCK_METHOD1(WriteUint32, bool(uint32_t value));
    MOCK_METHOD0(ReadUint64, uint64_t());
    MOCK_METHOD1(AddStubByIndex, uint64_t(IRemoteObject *stubObject));
    MOCK_METHOD1(CreateSoftbusServer, bool(const std::string &name));
    MOCK_METHOD1(AttachOrUpdateAppAuthInfo, bool(const AppAuthInfo &appAuthInfo));
    MOCK_METHOD1(DetachAppAuthInfo, bool(const AppAuthInfo &appAuthInfo));
};

static void *g_interface = nullptr;

IpcObjectStubInterfaceMock::IpcObjectStubInterfaceMock()
{
    g_interface = reinterpret_cast<void *>(this);
}

IpcObjectStubInterfaceMock::~IpcObjectStubInterfaceMock()
{
    g_interface = nullptr;
}

static IpcObjectStubInterface *GetIpcObjectStubInterface()
{
    return reinterpret_cast<IpcObjectStubInterface *>(g_interface);
}
extern "C" {
    bool Parcel::ReadString16Vector(std::vector<std::u16string> *val)
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return false;
        }
        return interface->ReadString16Vector(val);
    }
    bool Parcel::WriteInt32(int32_t value)
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return false;
        }
        return interface->WriteInt32(value);
    }
    bool Parcel::WriteString16(const std::u16string &value)
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return false;
        }
        return interface->WriteString16(value);
    }
    bool Parcel::WriteString(const std::string &value)
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return false;
        }
        return interface->WriteString(value);
    }
    pid_t IPCSkeleton::GetCallingPid()
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return -1;
        }
        return interface->GetCallingPid();
    }
    pid_t IPCSkeleton::GetCallingUid()
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return -1;
        }
        return interface->GetCallingUid();
    }
    uint32_t IPCSkeleton::GetCallingTokenID()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return 0;
        }
        return GetIpcObjectStubInterface()->GetCallingTokenID();
    }
    std::string IPCSkeleton::GetCallingDeviceID()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return "";
        }
        return GetIpcObjectStubInterface()->GetCallingDeviceID();
    }
    bool IPCSkeleton::IsLocalCalling()
    {
        IpcObjectStubInterface* interface = GetIpcObjectStubInterface();
        if (interface == nullptr) {
            return false;
        }
        return interface->IsLocalCalling();
    }
    int MessageParcel::ReadFileDescriptor()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return -1;
        }
        return GetIpcObjectStubInterface()->ReadFileDescriptor();
    }
    sptr<IRemoteObject> IPCProcessSkeleton::GetSAMgrObject()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return nullptr;
        }
        return GetIpcObjectStubInterface()->GetSAMgrObject();
    }
    std::string IPCObjectProxy::GetGrantedSessionName()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return "";
        }
        return GetIpcObjectStubInterface()->GetGrantedSessionName();
    }
    uint32_t Parcel::ReadUint32()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return 0;
        }
        return GetIpcObjectStubInterface()->ReadUint32();
    }
    const std::string Parcel::ReadString()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return "";
        }
        return GetIpcObjectStubInterface()->ReadString();
    }
    bool Parcel::WriteUint64(uint64_t value)
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return false;
        }
        return GetIpcObjectStubInterface()->WriteUint64(value);
    }
    bool Parcel::WriteUint32(uint32_t value)
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return false;
        }
        return GetIpcObjectStubInterface()->WriteUint32(value);
    }
    uint64_t Parcel::ReadUint64()
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return 0;
        }
        return GetIpcObjectStubInterface()->ReadUint64();
    }
    uint64_t IPCProcessSkeleton::AddStubByIndex(IRemoteObject *stubObject)
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return 0;
        }
        return GetIpcObjectStubInterface()->AddStubByIndex(stubObject);
    }
    bool IPCProcessSkeleton::CreateSoftbusServer(const std::string &name)
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return false;
        }
        return GetIpcObjectStubInterface()->CreateSoftbusServer(name);
    }
    bool IPCProcessSkeleton::AttachOrUpdateAppAuthInfo(const AppAuthInfo &appAuthInfo)
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return false;
        }
        return GetIpcObjectStubInterface()->AttachOrUpdateAppAuthInfo(appAuthInfo);
    }
    bool IPCProcessSkeleton::DetachAppAuthInfo(const AppAuthInfo &appAuthInfo)
    {
        if (GetIpcObjectStubInterface() == nullptr) {
            return false;
        }
        return GetIpcObjectStubInterface()->DetachAppAuthInfo(appAuthInfo);
    }
}

/**
 * @tc.name: OnRemoteDumpTest001
 * @tc.desc: Verify the OnRemoteDump function return IPC_STUB_INVALID_DATA_ERR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, OnRemoteDumpTest001, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    int fd = 1;
    data.WriteFileDescriptor(fd);
    NiceMock<IpcObjectStubInterfaceMock> mock;
    EXPECT_CALL(mock, ReadFileDescriptor).WillOnce(Return(-1));
    int result = stub->OnRemoteDump(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: OnRemoteDumpTest002
 * @tc.desc: Verify the OnRemoteDump function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, OnRemoteDumpTest002, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    int fd = 1;
    data.WriteFileDescriptor(fd);
    NiceMock<IpcObjectStubInterfaceMock> mock;
    EXPECT_CALL(mock, ReadFileDescriptor).WillOnce(Return(12));
    EXPECT_CALL(mock, ReadString16Vector).WillOnce(Return(false));
    int result = stub->OnRemoteDump(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: DBinderPingTransactionTest001
 * @tc.desc: Verify the DBinderPingTransaction function return IPC_STUB_WRITE_PARCEL_ERR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderPingTransactionTest001, TestSize.Level1)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    EXPECT_CALL(mock, WriteInt32).WillOnce(Return(false));
    int result = stub->DBinderPingTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_WRITE_PARCEL_ERR);
}

/**
 * @tc.name: DBinderSearchDescriptorTest001
 * @tc.desc: Verify the DBinderSearchDescriptor function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderSearchDescriptorTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, WriteString16).WillOnce(Return(true));
    int result = stub->DBinderSearchDescriptor(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: DBinderSearchDescriptorTest002
 * @tc.desc: Verify the DBinderSearchDescriptor function return IPC_STUB_WRITE_PARCEL_ERR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderSearchDescriptorTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, WriteString16).WillOnce(Return(false));
    int result = stub->DBinderSearchDescriptor(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_WRITE_PARCEL_ERR);
}

/**
 * @tc.name: DBinderDumpTransactionTest001
 * @tc.desc: Verify the DBinderDumpTransaction function when ReadFileDescriptor function return -1
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDumpTransactionTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderDatabusInvoker *invoker = new DBinderDatabusInvoker();
    invoker->SetCallerUid(0);
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = invoker;

    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(true));
    EXPECT_CALL(mock, ReadFileDescriptor).WillOnce(Return(-1));
    int result = stub->DBinderDumpTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    std::fill(current->invokers_, current->invokers_ + IPCThreadSkeleton::INVOKER_MAX_COUNT, nullptr);
    delete invoker;
}

/**
 * @tc.name: DBinderDumpTransactionTest002
 * @tc.desc: Verify the DBinderDumpTransaction function return ERR_NONE when (!IsLocalCalling) is true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDumpTransactionTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, GetCallingUid).WillOnce(Return(UID_TEST));
    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(false));
    auto result = stub->DBinderDumpTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: DBinderDumpTransactionTest003
 * @tc.desc: Verify the DBinderDumpTransaction function when GetCallingUid function return 0
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDumpTransactionTest003, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderDatabusInvoker *invoker = new DBinderDatabusInvoker();
    invoker->SetCallerUid(0);
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = invoker;

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, ReadFileDescriptor).WillOnce(Return(-1));
    auto result = stub->DBinderDumpTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    delete invoker;
}

/**
 * @tc.name: DBinderDumpTransactionTest004
 * @tc.desc: Verify the DBinderDumpTransaction function when GetCallingUid function return SHELL_UID
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDumpTransactionTest004, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderDatabusInvoker *invoker = new DBinderDatabusInvoker();
    invoker->SetCallerUid(SHELL_UID);
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = invoker;

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, ReadFileDescriptor).WillOnce(Return(-1));
    auto result = stub->DBinderDumpTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    std::fill(current->invokers_, current->invokers_ + IPCThreadSkeleton::INVOKER_MAX_COUNT, nullptr);
    delete invoker;
}

/**
 * @tc.name: DBinderDumpTransactionTest005
 * @tc.desc: Verify the DBinderDumpTransaction function when GetCallingUid function return HIDUMPER_SERVICE_UID
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDumpTransactionTest005, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderDatabusInvoker *invoker = new DBinderDatabusInvoker();
    invoker->SetCallerUid(UID_TEST);
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = invoker;

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    auto result = stub->DBinderDumpTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    std::fill(current->invokers_, current->invokers_ + IPCThreadSkeleton::INVOKER_MAX_COUNT, nullptr);
    delete invoker;
}

/**
 * @tc.name: DBinderIncRefsTransactionTest001
 * @tc.desc: Verify the DBinderIncRefsTransaction function IsLocalCalling function return true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderIncRefsTransactionTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, GetCallingPid).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, GetCallingTokenID).WillRepeatedly(Return(TOKEN_ID_TEST));
    EXPECT_CALL(mock, GetCallingDeviceID).WillRepeatedly(Return(DEVICEID_TEST));
    auto result = stub->DBinderIncRefsTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderIncRefsTransactionTest002
 * @tc.desc: Verify the DBinderIncRefsTransaction function current == nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderIncRefsTransactionTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;
    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(false));
    EXPECT_CALL(mock, GetCallingPid).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, GetCallingTokenID).WillRepeatedly(Return(TOKEN_ID_TEST));
    EXPECT_CALL(mock, GetCallingDeviceID).WillRepeatedly(Return(DEVICEID_TEST));
    auto result = stub->DBinderIncRefsTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: DBinderIncRefsTransactionTest003
 * @tc.desc: Verify the DBinderIncRefsTransaction function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderIncRefsTransactionTest003, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderDatabusInvoker *invoker = new DBinderDatabusInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = invoker;

    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(false));
    EXPECT_CALL(mock, AttachOrUpdateAppAuthInfo).WillOnce(Return(false));
    EXPECT_CALL(mock, GetCallingPid).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, GetCallingTokenID).WillRepeatedly(Return(TOKEN_ID_TEST));
    EXPECT_CALL(mock, GetCallingDeviceID).WillRepeatedly(Return(DEVICEID_TEST));
    auto result = stub->DBinderIncRefsTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    std::fill(current->invokers_, current->invokers_ + IPCThreadSkeleton::INVOKER_MAX_COUNT, nullptr);
    delete invoker;
}

/**
 * @tc.name: DBinderDecRefsTransactionTest001
 * @tc.desc: Verify the DBinderDecRefsTransaction function current == nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDecRefsTransactionTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(false));
    auto result = stub->DBinderDecRefsTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: DBinderDecRefsTransactionTest002
 * @tc.desc: Verify the DBinderDecRefsTransaction function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderDecRefsTransactionTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderDatabusInvoker *invoker = new DBinderDatabusInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = invoker;
    stub->IncStrongRef(stub.GetRefPtr());
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(false));
    EXPECT_CALL(mock, DetachAppAuthInfo).WillOnce(Return(true));
    auto result = stub->DBinderDecRefsTransaction(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    std::fill(current->invokers_, current->invokers_ + IPCThreadSkeleton::INVOKER_MAX_COUNT, nullptr);
    delete invoker;
}

/**
 * @tc.name: DBinderGetSessionNameTest001
 * @tc.desc: Verify the DBinderGetSessionName function IsLocalCalling is false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(false));
    auto result = stub->DBinderGetSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderGetSessionNameTest002
 * @tc.desc: Verify the DBinderGetSessionName function current == nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(true));
    auto result = stub->DBinderGetSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_CREATE_BUS_SERVER_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: DBinderGetSessionNameTest003
 * @tc.desc: Verify the DBinderGetSessionName function WriteString function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameTest003, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    sptr<IRemoteObject> object = new IPCObjectProxy(TEST_OBJECT_HANDLE, u"test");

    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(true));
    EXPECT_CALL(mock, GetSAMgrObject).WillOnce(Return(object));
    EXPECT_CALL(mock, GetGrantedSessionName).WillOnce(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(false));
    auto result = stub->DBinderGetSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderGetSessionNameTest004
 * @tc.desc: Verify the DBinderGetSessionName function WriteString function return true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameTest004, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    sptr<IRemoteObject> object = new IPCObjectProxy(TEST_OBJECT_HANDLE, u"test");
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(true));
    EXPECT_CALL(mock, GetSAMgrObject).WillOnce(Return(object));
    EXPECT_CALL(mock, GetGrantedSessionName).WillOnce(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));
    auto result = stub->DBinderGetSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: DBinderGetGrantedSessionNameTest001
 * @tc.desc: Verify the DBinderGetGrantedSessionName function IsLocalCalling function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetGrantedSessionNameTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(false));
    auto result = stub->DBinderGetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderGetGrantedSessionNameTest002
 * @tc.desc: Verify the DBinderGetGrantedSessionName function SetSamgrFlag function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetGrantedSessionNameTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(false);
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(true));
    auto result = stub->DBinderGetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderGetGrantedSessionNameTest003
 * @tc.desc: Verify the DBinderGetGrantedSessionName function SetSamgrFlag function return true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetGrantedSessionNameTest003, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(true);
    EXPECT_CALL(mock, IsLocalCalling).WillOnce(Return(true));
    auto result = stub->DBinderGetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->SetSamgrFlag(false);
}

/**
 * @tc.name: OnFirstStrongRefTest001
 * @tc.desc: Verify the OnFirstStrongRef function current->instance_ = nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, OnFirstStrongRefTest001, TestSize.Level1)
{
    const void* objectId = nullptr;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;
    ASSERT_NO_FATAL_FAILURE(stub->OnFirstStrongRef(objectId));
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: NoticeServiceDieTest001
 * @tc.desc: Verify the NoticeServiceDie function current->instance_ = nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, NoticeServiceDieTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data, reply;
    MessageOption option;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;
    int32_t result = stub->NoticeServiceDie(data, reply, option);
    EXPECT_EQ(result, IPC_STUB_CURRENT_NULL_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: NoticeServiceDieTest002
 * @tc.desc: Verify the NoticeServiceDie function return IPC_STUB_INVALID_DATA_ERR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, NoticeServiceDieTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data, reply;
    MessageOption option;
    IPCProcessSkeleton realObject;
    int32_t result = stub->NoticeServiceDie(data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: IsDeviceIdIllegalTest001
 * @tc.desc: Verify the IsDeviceIdIllegal function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, IsDeviceIdIllegalTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    std::string remoteDeviceId = DEVICEID_TEST;
    int32_t ret = stub->IsDeviceIdIllegal(remoteDeviceId);
    EXPECT_EQ(ret, false);
}

/**
 * @tc.name: GetSessionName001
 * @tc.desc: Verify the GetSessionName function current->instance_ = nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;
    auto ret = stub->GetSessionName();
    EXPECT_EQ(ret, "");
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: GetSessionName002
 * @tc.desc: Verify the GetSessionName function return ""
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IRemoteObject> object = new IPCObjectProxy(TEST_OBJECT_HANDLE, u"");

    EXPECT_CALL(mock, GetSAMgrObject).WillOnce(Return(object));

    auto ret = stub->GetSessionName();
    EXPECT_EQ(ret, "");
}

/**
 * @tc.name: GetSessionName003
 * @tc.desc: Verify the GetSessionName function return ""
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IRemoteObject> object = new IPCObjectProxy(TEST_OBJECT_HANDLE, u"test");

    EXPECT_CALL(mock, GetSAMgrObject).WillOnce(Return(object));
    EXPECT_CALL(mock, GetGrantedSessionName).WillOnce(Return(SESSION_NAME_TEST));

    auto ret = stub->GetSessionName();
    EXPECT_EQ(ret, SESSION_NAME_TEST);
}

/**
 * @tc.name: GetGrantedSessionNameTest001
 * @tc.desc: Verify the GetGrantedSessionName function return sessionName.empty
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetGrantedSessionNameTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    EXPECT_CALL(mock, GetCallingPid).WillOnce(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillOnce(Return(UID_TEST));
    int result = stub->GetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: GetGrantedSessionNameTest002
 * @tc.desc: Verify the GetGrantedSessionName function WriteUint32 function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetGrantedSessionNameTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };

    EXPECT_CALL(mock, GetCallingPid).WillOnce(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillOnce(Return(UID_TEST));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(false));

    int result = stub->GetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: GetGrantedSessionNameTest003
 * @tc.desc: Verify the GetGrantedSessionName function WriteString function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetGrantedSessionNameTest003, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };

    EXPECT_CALL(mock, GetCallingPid).WillOnce(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillOnce(Return(UID_TEST));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteString).WillOnce(Return(false));

    int result = stub->GetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: GetGrantedSessionNameTest004
 * @tc.desc: Verify the GetGrantedSessionName function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetGrantedSessionNameTest004, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };

    EXPECT_CALL(mock, GetCallingPid).WillOnce(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingUid).WillOnce(Return(UID_TEST));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteString).WillOnce(Return(true));

    int result = stub->GetGrantedSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = nullptr;
}

/**
 * @tc.name: AddAuthInfoTest001
 * @tc.desc: Verify the AddAuthInfo function return BINDER_CALLBACK_STUBINDEX_ERR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoTest001, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->exitFlag_ = true;

    EXPECT_CALL(mock, ReadString).WillOnce(Return(DEVICEID_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(REMOTEUID_TEST));
    EXPECT_CALL(mock, ReadUint64).WillOnce(Return(STUBINDEX_TEST2));
    int32_t ret = stub->AddAuthInfo(data, reply, CODE_TEST);
    EXPECT_EQ(ret, BINDER_CALLBACK_STUBINDEX_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: AddAuthInfoeTest002
 * @tc.desc: Verify the AddAuthInfoe function return IPC_STUB_CURRENT_NULL_ERR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoTest002, TestSize.Level1)
{
    NiceMock<IpcObjectStubInterfaceMock> mock;
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;

    EXPECT_CALL(mock, ReadString).WillOnce(Return(DEVICEID_TEST));

    int32_t ret = stub->AddAuthInfo(data, reply, CODE_TEST);
    EXPECT_EQ(ret, IPC_STUB_CURRENT_NULL_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: InvokerDataBusThreadTest001
 * @tc.desc: Verify the InvokerDataBusThread function deviceId is ""
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillOnce(Return(""))
        .WillOnce(Return(SESSION_NAME_TEST)).WillOnce(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest002
 * @tc.desc: Verify the InvokerDataBusThread function remoteDeviceId is ""
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillOnce(Return(SESSION_NAME_TEST))
        .WillOnce(Return("")).WillOnce(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest003
 * @tc.desc: Verify the InvokerDataBusThread function sessionName is ""
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillOnce(Return(SESSION_NAME_TEST))
        .WillOnce(Return(SESSION_NAME_TEST)).WillOnce(Return(""));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest004
 * @tc.desc: Verify the InvokerDataBusThread function return current->instance_ = nullptr
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest004, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->instance_ = nullptr;
    current->exitFlag_ = true;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_CURRENT_NULL_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: InvokerDataBusThreadTest005
 * @tc.desc: Verify the InvokerDataBusThread function create ""
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest005, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->exitFlag_ = true;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_CREATE_BUS_SERVER_ERR);
    current->instance_ = nullptr;
    current->exitFlag_ = false;
}

/**
 * @tc.name: InvokerDataBusThreadTest006
 * @tc.desc: Verify the InvokerDataBusThread function AddStubByIndex function return 0
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest006, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(0));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest007
 * @tc.desc: Verify the InvokerDataBusThread function WriteUint64 function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest007, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(false));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest008
 * @tc.desc: Verify the InvokerDataBusThread function WriteString function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest008, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(true));
    EXPECT_CALL(mock, WriteString).WillOnce(Return(false)).WillRepeatedly(Return(true));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest009
 * @tc.desc: Verify the InvokerDataBusThread function WriteString function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest009, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(true));
    EXPECT_CALL(mock, WriteString).WillOnce(Return(true)).WillRepeatedly(Return(false));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest0010
 * @tc.desc: Verify the InvokerDataBusThread function WriteUint32 function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest010, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(true));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteUint32).WillOnce(Return(false));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: InvokerDataBusThreadTest0011
 * @tc.desc: Verify the InvokerDataBusThread function AttachOrUpdateAppAuthInfo function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest011, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(true));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteUint32).WillOnce(Return(true));
    EXPECT_CALL(mock, AttachOrUpdateAppAuthInfo).WillOnce(Return(false));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, ERR_NONE);
    current->appInfoToStubIndex_.clear();
}

/**
 * @tc.name: InvokerDataBusThreadTest0012
 * @tc.desc: Verify the InvokerDataBusThread function AttachOrUpdateAppAuthInfo function return true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThreadTest012, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(true));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteUint32).WillOnce(Return(true));
    EXPECT_CALL(mock, AttachOrUpdateAppAuthInfo).WillOnce(Return(true));

    auto ret = stub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, ERR_NONE);
    current->appInfoToStubIndex_.clear();
}

/**
 * @tc.name: DBinderGetSessionNameForPidUidTest001
 * @tc.desc: Verify the DBinderGetSessionNameForPidUid function IsLocalCalling function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameForPidUidTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    NiceMock<IpcObjectStubInterfaceMock> mock;

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(false));

    auto ret = stub->DBinderGetSessionNameForPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderGetSessionNameForPidUidTest002
 * @tc.desc: Verify the DBinderGetSessionNameForPidUid function IsLocalCalling function return true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameForPidUidTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(false);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));

    auto ret = stub->DBinderGetSessionNameForPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderGetSessionNameForPidUidTest003
 * @tc.desc: Verify the DBinderGetSessionNameForPidUid function SetSamgrFlag function return true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetSessionNameForPidUidTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(true);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, GetCallingPid).WillOnce(Return(PID_TEST));

    auto ret = stub->DBinderGetSessionNameForPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    current->SetSamgrFlag(false);
}

/**
 * @tc.name: GetSessionNameForPidUidTest001
 * @tc.desc: Verify the GetSessionNameForPidUid function WriteUint32 function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameForPidUidTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };

    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, GetCallingPid).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(false));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));

    auto ret = stub->GetSessionNameForPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = nullptr;
}

/**
 * @tc.name: GetSessionNameForPidUidTest002
 * @tc.desc: Verify the GetSessionNameForPidUid function WriteString function return false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameForPidUidTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };

    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, GetCallingPid).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(false));

    auto ret = stub->GetSessionNameForPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = nullptr;
}

/**
 * @tc.name: GetSessionNameForPidUidTest003
 * @tc.desc: Verify the GetSessionNameForPidUid function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameForPidUidTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    NiceMock<IpcObjectStubInterfaceMock> mock;
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };

    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(UID_TEST));
    EXPECT_CALL(mock, GetCallingPid).WillRepeatedly(Return(PID_TEST));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));

    auto ret = stub->GetSessionNameForPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, ERR_NONE);
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = nullptr;
}

/**
 * @tc.name: CreateSessionNameTest001
 * @tc.desc: Verify the CreateSessionName function return valid
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, CreateSessionNameTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = [](int32_t, int32_t, const char*) { return 0; };
    std::string sessionName = DBINDER_SOCKET_NAME_PREFIX + std::to_string(UID_TEST)
        + std::string("_") + std::to_string(PID_TEST);

    auto ret = stub->CreateSessionName(UID_TEST, PID_TEST);
    EXPECT_EQ(ret, sessionName);
    DBinderSoftbusClient::GetInstance().grantPermissionFunc_ = nullptr;
}

/**
 * @tc.name: RemoveSessionNameTest001
 * @tc.desc: Verify the RemoveSessionName function return valid
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, RemoveSessionNameTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    DBinderSoftbusClient::GetInstance().removePermissionFunc_ = [](const char *) { return 0; };
    std::string sessionName = DBINDER_SOCKET_NAME_PREFIX + std::to_string(UID_TEST)
        + std::string("_") + std::to_string(PID_TEST);

    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(sessionName));

    auto ret = stub->RemoveSessionName(data);
    EXPECT_EQ(ret, ERR_NONE);
    DBinderSoftbusClient::GetInstance().removePermissionFunc_ = nullptr;
}

/**
 * @tc.name: DBinderGetPidUidTest001
 * @tc.desc: Verify the DBinderGetPidUid function return WriteUint32 is false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetPidUidTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(true);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(false));

    auto ret = stub->DBinderGetPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    current->SetSamgrFlag(false);
}

/**
 * @tc.name: DBinderGetPidUidTest002
 * @tc.desc: Verify the DBinderGetPidUid function return WriteUint32 is true
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderGetPidUidTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(true);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));

    auto ret = stub->DBinderGetPidUid(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, ERR_NONE);
    current->SetSamgrFlag(false);
}

/**
 * @tc.name: DBinderRemoveSessionNameTest001
 * @tc.desc: Verify the DBinderRemoveSessionName function return IsLocalCalling is false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderRemoveSessionNameTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(true);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(false));

    auto ret = stub->DBinderRemoveSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    current->SetSamgrFlag(false);
}

/**
 * @tc.name: DBinderRemoveSessionNameTest002
 * @tc.desc: Verify the DBinderRemoveSessionName function return IsSamgrCall is false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderRemoveSessionNameTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(false);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));

    auto ret = stub->DBinderRemoveSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: DBinderRemoveSessionNameTest003
 * @tc.desc: Verify the DBinderRemoveSessionName function RemoveSessionName function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, DBinderRemoveSessionNameTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto current = ProcessSkeleton::GetInstance();
    current->SetSamgrFlag(true);
    DBinderSoftbusClient::GetInstance().removePermissionFunc_ = [](const char *) { return 0; };
    std::string sessionName = DBINDER_SOCKET_NAME_PREFIX + std::to_string(UID_TEST)
        + std::string("_") + std::to_string(PID_TEST);

    EXPECT_CALL(mock, IsLocalCalling).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(sessionName));

    auto ret = stub->DBinderRemoveSessionName(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, ERR_NONE);
    DBinderSoftbusClient::GetInstance().removePermissionFunc_ = nullptr;
    current->SetSamgrFlag(false);
}

/**
 * @tc.name: ProcessProtoTest001
 * @tc.desc: Verify the ProcessProto function return WriteUint32 is false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, ProcessProtoTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(false));

    auto ret = stub->ProcessProto(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_WRITE_PARCEL_ERR);
}

/**
 * @tc.name: ProcessProtoTest002
 * @tc.desc: Verify the ProcessProto function return WriteString16 is false
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, ProcessProtoTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteString16).WillRepeatedly(Return(false));

    auto ret = stub->ProcessProto(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_WRITE_PARCEL_ERR);
}

/**
 * @tc.name: ProcessProtoTest003
 * @tc.desc: Verify the ProcessProto function return ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, ProcessProtoTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    EXPECT_CALL(mock, WriteUint32).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteString16).WillRepeatedly(Return(true));

    auto ret = stub->ProcessProto(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, ERR_NONE);
}

/**
 * @tc.name: InvokerThreadTest001
 * @tc.desc: Verify the InvokerThread function InvokerDataBusThread functionreturn ERR_NONE
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerThreadTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();

    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(IRemoteObject::DATABUS_TYPE));
    EXPECT_CALL(mock, ReadString).WillRepeatedly(Return(SESSION_NAME_TEST));
    EXPECT_CALL(mock, CreateSoftbusServer).WillOnce(Return(true));
    EXPECT_CALL(mock, AddStubByIndex).WillOnce(Return(10));
    EXPECT_CALL(mock, WriteUint64).WillOnce(Return(true));
    EXPECT_CALL(mock, WriteString).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, WriteUint32).WillOnce(Return(true));
    EXPECT_CALL(mock, AttachOrUpdateAppAuthInfo).WillOnce(Return(false));

    auto ret = stub->InvokerThread(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, ERR_NONE);
    current->appInfoToStubIndex_.clear();
}

/**
 * @tc.name: InvokerThreadTest002
 * @tc.desc: Verify the InvokerThread function ReadUint32 function return IRemoteObject::IF_PROT_ERROR
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerThreadTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> stub = new IPCObjectStub(u"testObjectStub");
    NiceMock<IpcObjectStubInterfaceMock> mock;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();

    EXPECT_CALL(mock, ReadUint32).WillRepeatedly(Return(IRemoteObject::IF_PROT_ERROR));

    auto ret = stub->InvokerThread(CODE_TEST, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    current->appInfoToStubIndex_.clear();
}
}