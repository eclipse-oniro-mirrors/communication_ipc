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

#include "dbinder_session_object.h"
#include "ipc_types.h"
#include "mock_session_impl.h"

using namespace testing::ext;
using namespace OHOS;

namespace {
const std::string SERVICE_TEST = "serviceTest";
const std::string DEVICE_TEST = "deviceTest";
const std::string LOCALDEVICE_TEST = "localDeviceTest";
}

class DBinderSessionObjectTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void DBinderSessionObjectTest::SetUpTestCase()
{
}

void DBinderSessionObjectTest::TearDownTestCase()
{
}

void DBinderSessionObjectTest::SetUp()
{
}

void DBinderSessionObjectTest::TearDown()
{
}

/**
 * @tc.name: SetBusSessionTest001
 * @tc.desc: Verify the DBinderSessionObject::SetBusSession function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, SetBusSessionTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> sessionMock = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(nullptr, serviceName, serverDeviceId, 1, nullptr, 1);

    object.SetBusSession(sessionMock);
    auto session = object.GetBusSession();
    EXPECT_NE(session, nullptr);
}

/**
 * @tc.name: GetBusSessionTest001
 * @tc.desc: Verify the DBinderSessionObject::GetBusSession function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, GetBusSessionTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> sessionMock = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(sessionMock, serviceName, serverDeviceId, 1, nullptr, 1);

    EXPECT_CALL(*sessionMock, GetChannelId())
        .WillRepeatedly(testing::Return(1));

    EXPECT_CALL(*sessionMock, GetSessionId())
        .WillRepeatedly(testing::Return(1));

    std::string deviceId = "DeviceId";
    EXPECT_CALL(*sessionMock, GetPeerDeviceId())
        .WillRepeatedly(testing::ReturnRef(deviceId));

    auto session = object.GetBusSession();
    EXPECT_NE(session, nullptr);
    object.CloseDatabusSession();
}

/**
 * @tc.name: GetSessionBuffTest001
 * @tc.desc: Verify the DBinderSessionObject::GetSessionBuff function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, GetSessionBuffTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> sessionMock = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(sessionMock, serviceName, serverDeviceId, 1, nullptr, 1);

    std::shared_ptr<BufferObject> buffer = object.GetSessionBuff();
    EXPECT_NE(buffer, nullptr);
}

/**
 * @tc.name: SetServiceNameTest001
 * @tc.desc: Verify the DBinderSessionObject::SetServiceName function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, SetServiceNameTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> session = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(session, serviceName, serverDeviceId, 1, nullptr, 1);

    std::string name = "testname";
    object.SetServiceName(name);
    EXPECT_STREQ(name.c_str(), object.GetServiceName().c_str());
}

/**
 * @tc.name: SetDeviceIdTest001
 * @tc.desc: Verify the DBinderSessionObject::SetDeviceId function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, SetDeviceIdTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> session = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(session, serviceName, serverDeviceId, 1, nullptr, 1);

    std::string deviceId = "testid";
    object.SetDeviceId(deviceId);
    EXPECT_STREQ(deviceId.c_str(), object.GetDeviceId().c_str());
}

/**
 * @tc.name: SetProxyTest001
 * @tc.desc: Verify the DBinderSessionObject::SetProxy function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, SetProxyTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> session = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    IPCObjectProxy *testProxy = new IPCObjectProxy(1, u"testproxy");
    DBinderSessionObject object(session, serviceName, serverDeviceId, 1, nullptr, 1);

    object.SetProxy(testProxy);
    EXPECT_NE(object.GetProxy(), nullptr);
}

/**
 * @tc.name: GetFlatSessionLenTest001
 * @tc.desc: Verify the DBinderSessionObject::GetFlatSessionLen function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, GetFlatSessionLenTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> session = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(session, serviceName, serverDeviceId, 1, nullptr, 1);

    uint32_t len = object.GetFlatSessionLen();
    EXPECT_EQ(sizeof(FlatDBinderSession), len);
}

/**
 * @tc.name: GetSessionHandleTest001
 * @tc.desc: Verify the DBinderSessionObject::GetSessionHandle function
 * @tc.type: FUNC
 */
HWTEST_F(DBinderSessionObjectTest, GetSessionHandleTest001, TestSize.Level1)
{
    std::shared_ptr<MockSessionImpl> sessionMock = std::make_shared<MockSessionImpl>();
    std::string serviceName = "testserviceName";
    std::string serverDeviceId = "testserverDeviceId";
    DBinderSessionObject object(sessionMock, serviceName, serverDeviceId, 1, nullptr, 1);

    EXPECT_CALL(*sessionMock, GetChannelId())
        .WillRepeatedly(testing::Return(1));

    uint32_t ret = object.GetSessionHandle();
    uint32_t id = 1;
    EXPECT_EQ(ret, id);
}