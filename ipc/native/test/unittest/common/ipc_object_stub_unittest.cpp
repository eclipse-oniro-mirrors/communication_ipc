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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define private public
#define protected public
#include "comm_auth_info.h"
#include "databus_session_callback.h"
#include "dbinder_databus_invoker.h"
#include "dbinder_session_object.h"
#include "binder_invoker.h"
#include "ipc_debug.h"
#include "ipc_skeleton.h"
#include "ipc_object_proxy.h"
#include "ipc_object_stub.h"
#include "ipc_process_skeleton.h"
#include "ipc_thread_skeleton.h"
#include "dbinder_session_object.h"
#include "message_option.h"
#include "mock_session_impl.h"
#include "mock_iremote_invoker.h"
#undef protected
#undef private

using namespace testing::ext;
using namespace OHOS;

namespace {
constexpr pid_t ALLOWED_UID = 10000;
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

/**
 * @tc.name: OnRemoteDumpTest001
 * @tc.desc: Verify the OnRemoteDump function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, OnRemoteDumpTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = 0;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->OnRemoteDump(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: SendRequestTest001
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = INTERFACE_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: SendRequestTest002
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = SYNCHRONIZE_REFERENCE;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: SendRequestTest003
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DUMP_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: SendRequestTest004
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest004, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_PROTO_INFO;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

#ifndef CONFIG_IPC_SINGLE
/**
 * @tc.name: SendRequestTest005
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest005, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = INVOKE_LISTEN_THREAD;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVOKE_THREAD_ERR);
}

/**
 * @tc.name: SendRequestTest006
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest006, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = INVOKE_LISTEN_THREAD;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    BinderInvoker *invoker = new BinderInvoker();
    invoker->status_ = IRemoteInvoker::ACTIVE_INVOKER;
    invoker->callerUid_ = ALLOWED_UID;
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: SendRequestTest007
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest007, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_INCREFS_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    BinderInvoker *invoker = new BinderInvoker();
    invoker->status_ = IRemoteInvoker::ACTIVE_INVOKER;
    invoker->callerUid_ = ALLOWED_UID;
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: SendRequestTest008
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest008, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_DECREFS_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: SendRequestTest009
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest009, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_DECREFS_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    DBinderDatabusInvoker *dbinderInvoker = new DBinderDatabusInvoker();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = dbinderInvoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, IsLocalCalling())
        .WillRepeatedly(testing::Return(false));

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    current->invokers_.clear();
    delete invoker;
    delete dbinderInvoker;
}

/**
 * @tc.name: SendRequestTest010
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest010, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_ADD_COMMAUTH;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: SendRequestTest011
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest011, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_PID_UID;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;
    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, IsLocalCalling())
        .WillRepeatedly(testing::Return(true));

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    delete invoker;
    current->invokers_.clear();
}

/**
 * @tc.name: SendRequestTest012
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest012, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_SESSION_NAME;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
}

/**
 * @tc.name: SendRequestTest013
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest013, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_GRANTED_SESSION_NAME;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: SendRequestTest014
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest014, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_SESSION_NAME_PID_UID;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: SendRequestTest015
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest015, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_SESSION_NAME;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, IsLocalCalling())
        .WillRepeatedly(testing::Return(false));

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: SendRequestTest016
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest016, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_ADD_COMMAUTH;
    MessageParcel reply;
    MessageOption option;
    MessageParcel data;

    uint32_t remotePid =  1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testRemoteDeviceId";
    data.WriteString(remoteDeviceId);
    uint32_t remoteFeature =  1;
    data.WriteUint32(remoteFeature);
    uint64_t stubIndex = 1;
    data.WriteUint64(stubIndex);

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    DBinderDatabusInvoker *dbinderInvoker = new DBinderDatabusInvoker();
    current->invokers_[IRemoteObject::IF_PROT_DATABUS] = dbinderInvoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, IsLocalCalling())
        .WillRepeatedly(testing::Return(false));

    EXPECT_CALL(*invoker, GetCallerUid())
        .WillRepeatedly(testing::Return(ALLOWED_UID - 1));

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_NE(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: SendRequestTest017
 * @tc.desc: Verify the SendRequest function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, SendRequestTest017, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = GET_PID_UID;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;
    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, IsLocalCalling())
        .WillRepeatedly(testing::Return(false));

    int result = testStub->SendRequest(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}
#endif

#ifndef CONFIG_IPC_SINGLE

/**
 * @tc.name: InvokerDataBusThread001
 * @tc.desc: Verify the InvokerDataBusThread function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThread001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    MessageParcel data;
    MessageParcel reply;

    std::string deviceId = "testdeviceId";
    data.WriteString(deviceId);
    uint32_t remotePid = 1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid = 1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testremoteDeviceId";
    data.WriteString(remoteDeviceId);
    std::string sessionName = "testsessionName";
    data.WriteString(sessionName);
    uint32_t featureSet = 1;
    data.WriteUint32(featureSet);

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->stubObjects_[0] = testStub.GetRefPtr();
    auto ret = testStub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    current->stubObjects_.clear();
}

/**
 * @tc.name: InvokerDataBusThread002
 * @tc.desc: Verify the InvokerDataBusThread function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThread002, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    MessageParcel data;
    MessageParcel reply;
    std::string deviceId = "testdeviceId";
    data.WriteString(deviceId);
    uint32_t remotePid = 1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid = 1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testremoteDeviceId";
    data.WriteString(remoteDeviceId);
    std::string sessionName = "testsessionName";
    data.WriteString(sessionName);
    uint32_t featureSet = 1;
    data.WriteUint32(featureSet);

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->stubObjects_[1] = testStub.GetRefPtr();

    std::string appInfo = remoteDeviceId +
        std::to_string(remotePid) + std::to_string(remoteUid);
    current->appInfoToStubIndex_[appInfo] = std::map<uint64_t, uint32_t> { { 1, 1 } };

    auto ret = testStub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, ERR_NONE);
    current->stubObjects_.clear();
    current->appInfoToStubIndex_.clear();
}

/**
 * @tc.name: InvokerDataBusThread003
 * @tc.desc: Verify the InvokerDataBusThread function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThread003, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    MessageParcel data;
    MessageParcel reply;
    std::string deviceId = "testdeviceId";
    data.WriteString(deviceId);
    uint32_t remotePid = 1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid = 1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testremoteDeviceId";
    data.WriteString(remoteDeviceId);
    std::string sessionName = "testsessionName";
    data.WriteString(sessionName);
    uint32_t featureSet = 1;
    data.WriteUint32(featureSet);

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->stubObjects_[1] = testStub.GetRefPtr();

    std::string appInfo = remoteDeviceId +
        std::to_string(remotePid) + std::to_string(remoteUid);
    current->appInfoToStubIndex_[appInfo] = std::map<uint64_t, uint32_t> { { 0, 1 } };

    auto ret = testStub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, ERR_NONE);
    current->stubObjects_.clear();
    current->appInfoToStubIndex_.clear();
}

/**
 * @tc.name: InvokerDataBusThread004
 * @tc.desc: Verify the InvokerDataBusThread function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerDataBusThread004, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    MessageParcel data;
    MessageParcel reply;

    std::string deviceId = "";
    data.WriteString(deviceId);
    uint32_t remotePid = 1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid = 1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testremoteDeviceId";
    data.WriteString(remoteDeviceId);
    std::string sessionName = "testsessionName";
    data.WriteString(sessionName);
    uint32_t featureSet = 1;
    data.WriteUint32(featureSet);

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->stubObjects_[0] = testStub.GetRefPtr();
    auto ret = testStub->InvokerDataBusThread(data, reply);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
    current->stubObjects_.clear();
}

/**
 * @tc.name: InvokerThread001
 * @tc.desc: Verify the InvokerThread function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, InvokerThread001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    uint32_t code = 1;
    MessageParcel data;
    MessageParcel reply;
    uint32_t type = IRemoteObject::DATABUS_TYPE;
    data.WriteUint32(type);
    std::string deviceId = "";
    data.WriteString(deviceId);
    uint32_t remotePid = 1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid = 1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "";
    data.WriteString(remoteDeviceId);
    std::string sessionName = "";
    data.WriteString(sessionName);
    uint32_t featureSet = 1;
    data.WriteUint32(featureSet);
    MessageOption option;

    auto ret = testStub->InvokerThread(code, data, reply, option);
    EXPECT_EQ(ret, IPC_STUB_INVOKE_THREAD_ERR);
}

/**
 * @tc.name: NoticeServiceDieTest001
 * @tc.desc: Verify the NoticeServiceDie function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, NoticeServiceDieTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    sptr<IPCObjectProxy> objectProxy = new IPCObjectProxy(
        1, u"test", IPCProcessSkeleton::DBINDER_HANDLE_BASE);
    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    current->noticeStub_[objectProxy.GetRefPtr()] = testStub;

    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto result = testStub->NoticeServiceDie(data, reply, option);
    EXPECT_EQ(result, ERR_NONE);
    current->noticeStub_.clear();
}

/**
 * @tc.name: NoticeServiceDieTest002
 * @tc.desc: Verify the NoticeServiceDie function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, NoticeServiceDieTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    auto result = testStub->NoticeServiceDie(data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: AddAuthInfoeTest001
 * @tc.desc: Verify the AddAuthInfoe function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoeTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    MessageParcel data;
    MessageParcel reply;
    uint32_t code = 0;
    int32_t ret = testStub->AddAuthInfo(data, reply, code);
    EXPECT_EQ(ret, IPC_STUB_INVALID_DATA_ERR);
}

/**
 * @tc.name: AddAuthInfoeTest002
 * @tc.desc: Verify the AddAuthInfoe function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoeTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    MessageParcel data;
    uint32_t remotePid =  1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testRemoteDeviceId";
    data.WriteString(remoteDeviceId);
    uint32_t remoteFeature =  1;
    data.WriteUint32(remoteFeature);
    uint64_t stubIndex = 1;
    data.WriteUint64(stubIndex);

    uint32_t tokenId = 1;
    MessageParcel reply;
    uint32_t code = DBINDER_ADD_COMMAUTH;

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    std::shared_ptr<CommAuthInfo> info =
        std::make_shared<CommAuthInfo>(testStub.GetRefPtr(), remotePid, remoteUid, tokenId, remoteDeviceId);
    current->commAuth_.push_back(info);

    int32_t ret = testStub->AddAuthInfo(data, reply, code);
    EXPECT_EQ(ret, ERR_NONE);
    info->stub_ = nullptr;
    current->commAuth_.remove(info);
}

/**
 * @tc.name: AddAuthInfoeTest003
 * @tc.desc: Verify the AddAuthInfoe function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoeTest003, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    MessageParcel data;
    uint32_t remotePid =  1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testRemoteDeviceId";
    data.WriteString(remoteDeviceId);
    uint32_t remoteFeature =  1;
    data.WriteUint32(remoteFeature);
    uint64_t stubIndex = 1;
    data.WriteUint64(stubIndex);

    MessageParcel reply;
    uint32_t code = DBINDER_ADD_COMMAUTH;

    int32_t ret = testStub->AddAuthInfo(data, reply, code);
    EXPECT_EQ(ret, ERR_NONE);
}

/**
 * @tc.name: AddAuthInfoeTest004
 * @tc.desc: Verify the AddAuthInfoe function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoeTest004, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    MessageParcel data;
    uint32_t remotePid =  1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testRemoteDeviceId";
    data.WriteString(remoteDeviceId);
    uint32_t remoteFeature =  1;
    data.WriteUint32(remoteFeature);
    uint64_t stubIndex = 1;
    data.WriteUint64(stubIndex);
    uint32_t tokenId = 1;
    MessageParcel reply;
    uint32_t code = DBINDER_ADD_COMMAUTH;

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    std::shared_ptr<CommAuthInfo> info =
        std::make_shared<CommAuthInfo>(testStub.GetRefPtr(), remotePid, remoteUid, tokenId, remoteDeviceId);
    current->commAuth_.push_back(info);

    int32_t ret = testStub->AddAuthInfo(data, reply, code);
    EXPECT_EQ(ret, ERR_NONE);
    info->stub_ = nullptr;
    current->commAuth_.remove(info);
}

/**
 * @tc.name: AddAuthInfoeTest005
 * @tc.desc: Verify the AddAuthInfoe function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoeTest005, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    MessageParcel data;
    uint32_t remotePid =  1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testRemoteDeviceId";
    data.WriteString(remoteDeviceId);
    uint32_t remoteFeature =  1;
    data.WriteUint32(remoteFeature);
    uint64_t stubIndex = 0;
    data.WriteUint64(stubIndex);
    uint32_t tokenId = 1;
    MessageParcel reply;
    uint32_t code = DBINDER_ADD_COMMAUTH;

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    std::shared_ptr<CommAuthInfo> info =
        std::make_shared<CommAuthInfo>(testStub.GetRefPtr(), remotePid, remoteUid, tokenId, remoteDeviceId);
    current->commAuth_.push_back(info);

    int32_t ret = testStub->AddAuthInfo(data, reply, code);
    EXPECT_EQ(ret, ERR_NONE);
    info->stub_ = nullptr;
    current->commAuth_.remove(info);
}

/**
 * @tc.name: AddAuthInfoeTest006
 * @tc.desc: Verify the AddAuthInfoe function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, AddAuthInfoeTest006, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    MessageParcel data;
    uint32_t remotePid =  1;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1;
    data.WriteUint32(remoteUid);
    std::string remoteDeviceId = "testRemoteDeviceId";
    data.WriteString(remoteDeviceId);
    uint32_t remoteFeature =  1;
    data.WriteUint32(remoteFeature);
    uint64_t stubIndex = 1;
    data.WriteUint64(stubIndex);
    uint32_t tokenId = 1;
    MessageParcel reply;
    uint32_t code = DBINDER_ADD_COMMAUTH;

    IPCProcessSkeleton *current = IPCProcessSkeleton::GetCurrent();
    std::shared_ptr<CommAuthInfo> info =
        std::make_shared<CommAuthInfo>(testStub.GetRefPtr(), remotePid, remoteUid, tokenId, remoteDeviceId);
    current->commAuth_.push_back(info);

    std::string appInfo = remoteDeviceId + std::to_string(remotePid) + std::to_string(remoteUid);
    current->appInfoToStubIndex_[appInfo] = std::map<uint64_t, uint32_t> { { 0, 1 } };

    int32_t ret = testStub->AddAuthInfo(data, reply, code);
    EXPECT_EQ(ret, ERR_NONE);
    info->stub_ = nullptr;
    current->commAuth_.remove(info);
}

/**
 * @tc.name: IsSamgrCallTest001
 * @tc.desc: Verify the IsSamgrCall function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, IsSamgrCallTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    auto ret = testStub->IsSamgrCall();
    ASSERT_FALSE(ret);
}

/**
 * @tc.name: GetGrantedSessionNameTest001
 * @tc.desc: Verify the GetGrantedSessionName function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetGrantedSessionNameTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_DECREFS_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, GetCallerPid())
        .WillRepeatedly(testing::Return(1111));

    EXPECT_CALL(*invoker, GetCallerUid())
        .WillRepeatedly(testing::Return(1112));

    EXPECT_CALL(*invoker, SendRequest(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(1));

    int result = testStub->GetGrantedSessionName(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: GetSessionNameForPidUidTest001
 * @tc.desc: Verify the GetSessionNameForPidUid function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameForPidUidTest001, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_DECREFS_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    uint32_t remotePid =  1111;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1112;
    data.WriteUint32(remoteUid);

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, GetCallerPid())
        .WillRepeatedly(testing::Return(1111));

    EXPECT_CALL(*invoker, GetCallerUid())
        .WillRepeatedly(testing::Return(1112));

    EXPECT_CALL(*invoker, SendRequest(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(1));

    int result = testStub->GetSessionNameForPidUid(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: GetSessionNameForPidUidTest002
 * @tc.desc: Verify the GetSessionNameForPidUid function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetSessionNameForPidUidTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");
    uint32_t code = DBINDER_DECREFS_TRANSACTION;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    uint32_t remotePid =  1111;
    data.WriteUint32(remotePid);
    uint32_t remoteUid =  1112;
    data.WriteUint32(remoteUid);

    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, GetCallerPid())
        .WillRepeatedly(testing::Return(1113));

    EXPECT_CALL(*invoker, GetCallerUid())
        .WillRepeatedly(testing::Return(1114));

    EXPECT_CALL(*invoker, SendRequest(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(1));

    int result = testStub->GetSessionNameForPidUid(code, data, reply, option);
    EXPECT_EQ(result, IPC_STUB_INVALID_DATA_ERR);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: CreateSessionNameTest002
 * @tc.desc: Verify the CreateSessionName function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, CreateSessionNameTest002, TestSize.Level1)
{
    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    int uid = 1111;
    int pid = 1112;

    auto ret = testStub->CreateSessionName(uid, pid);
    ASSERT_TRUE(ret.size() == 0);
}
#endif

/**
 * @tc.name: GetCallingTokenIDTest001
 * @tc.desc: Verify the GetCallingTokenID function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetCallingTokenIDTest001, TestSize.Level1)
{
    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, GetCallerTokenID())
        .WillRepeatedly(testing::Return(1));

    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    auto ret = testStub->GetCallingTokenID();
    EXPECT_EQ(ret, 1);
    current->invokers_.clear();
    delete invoker;
}

/**
 * @tc.name: GetCallingFullTokenIDTest001
 * @tc.desc: Verify the GetCallingFullTokenID function
 * @tc.type: FUNC
 */
HWTEST_F(IPCObjectStubTest, GetCallingFullTokenIDTest001, TestSize.Level1)
{
    MockIRemoteInvoker *invoker = new MockIRemoteInvoker();
    IPCThreadSkeleton *current = IPCThreadSkeleton::GetCurrent();
    current->invokers_[IRemoteObject::IF_PROT_BINDER] = invoker;

    EXPECT_CALL(*invoker, GetStatus())
        .WillRepeatedly(testing::Return(IRemoteInvoker::ACTIVE_INVOKER));

    EXPECT_CALL(*invoker, GetCallerTokenID())
        .WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*invoker, GetSelfTokenID())
        .WillRepeatedly(testing::Return(1));

    sptr<IPCObjectStub> testStub = new IPCObjectStub(u"testStub");

    auto ret = testStub->GetCallingFullTokenID();
    EXPECT_EQ(ret, 1);
    current->invokers_.clear();
    delete invoker;
}