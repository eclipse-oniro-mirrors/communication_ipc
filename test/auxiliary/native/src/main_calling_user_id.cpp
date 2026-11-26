/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <nativetoken_kit.h>
#include <token_setproc.h>

#include "sys_binder.h"
#include "test_service_client.h"

using namespace OHOS;

namespace {
constexpr int CONNECT_RETRY_TIMES = 20;
constexpr useconds_t CONNECT_RETRY_INTERVAL_US = 200 * 1000;
constexpr char TMP_SERVER_PATH[] = "/data/local/tmp/ipc_server_test";
constexpr char SYSTEM_SERVER_PATH[] = "/system/bin/ipc_server_test";
constexpr char BINDER_DRIVER_PATH[] = "/dev/binder";
constexpr uid_t TEST_CALLER_UID = 20000000;

void InitTokenId()
{
    NativeTokenInfoParams infoInstance = {
        .dcapsNum = 0,
        .permsNum = 0,
        .aclsNum = 0,
        .dcaps = nullptr,
        .perms = nullptr,
        .acls = nullptr,
        .processName = "com.ipc.calling_user_id.test",
        .aplStr = "normal",
    };
    uint64_t tokenId = GetAccessTokenId(&infoInstance);
    SetSelfTokenID(tokenId);
}

const char *GetServerPath()
{
    if (access(TMP_SERVER_PATH, X_OK) == 0) {
        return TMP_SERVER_PATH;
    }
    return SYSTEM_SERVER_PATH;
}

pid_t StartServer(const char *serverPath)
{
    pid_t pid = fork();
    if (pid != 0) {
        return pid;
    }

    execl(serverPath, serverPath, nullptr);
    _exit(EXIT_FAILURE);
}

void StopServer(pid_t pid)
{
    if (pid <= 0) {
        return;
    }
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
}

bool ConnectWithRetry(std::shared_ptr<TestServiceClient> &client)
{
    for (int i = 0; i < CONNECT_RETRY_TIMES; i++) {
        if (client->ConnectService()) {
            return true;
        }
        usleep(CONNECT_RETRY_INTERVAL_US);
    }
    return false;
}

bool QueryCallingUserIDSupport(bool &supported)
{
    supported = false;
    int fd = open(BINDER_DRIVER_PATH, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        std::cout << "[FAILED] Open " << BINDER_DRIVER_PATH << " failed, errno = " << errno << std::endl;
        return false;
    }

    uint64_t featureSet = 0;
    if (ioctl(fd, BINDER_FEATURE_SET, &featureSet) != 0) {
        std::cout << "[FAILED] Query binder feature set failed, errno = " << errno << std::endl;
        close(fd);
        return false;
    }
    close(fd);

    supported = (featureSet & CALLING_USER_INFO_FEATURE_MASK) != 0;
    std::cout << "Binder feature set: 0x" << std::hex << featureSet << std::dec
              << ", calling_user_info supported: " << (supported ? "true" : "false") << std::endl;
    return true;
}

int RunClient()
{
    InitTokenId();
    if (setuid(TEST_CALLER_UID) != 0) {
        std::cout << "[FAILED] Switch caller uid failed, errno = " << errno << std::endl;
        return EXIT_FAILURE;
    }
    auto client = std::make_shared<TestServiceClient>();
    if (!ConnectWithRetry(client)) {
        std::cout << "[FAILED] Connect ipc_server_test failed" << std::endl;
        return EXIT_FAILURE;
    }
#ifdef CALLING_USER_INFO_ENABLED
    if (!client->TestCallingUserID()) {
        return EXIT_FAILURE;
    }
#endif // CALLING_USER_INFO_ENABLED

    return EXIT_SUCCESS;
}
} // namespace

int main()
{
    InitTokenId();
    bool supported = false;
    if (!QueryCallingUserIDSupport(supported)) {
        return EXIT_FAILURE;
    }
    if (!supported) {
        std::cout << "[IPC_NOT_SUPPORT] BINDER_GET_CALLING_USER_INFO is not supported by current device"
                  << std::endl;
        return EXIT_SUCCESS;
    }

    const char *serverPath = GetServerPath();
    pid_t serverPid = StartServer(serverPath);
    if (serverPid < 0) {
        std::cout << "[FAILED] Start ipc_server_test failed" << std::endl;
        return EXIT_FAILURE;
    }

    pid_t clientPid = fork();
    if (clientPid < 0) {
        std::cout << "[FAILED] Start client process failed" << std::endl;
        StopServer(serverPid);
        return EXIT_FAILURE;
    }
    if (clientPid == 0) {
        return RunClient();
    }

    int clientStatus = 0;
    waitpid(clientPid, &clientStatus, 0);
    StopServer(serverPid);

    if (!WIFEXITED(clientStatus) || WEXITSTATUS(clientStatus) != EXIT_SUCCESS) {
        std::cout << "[FAILED] Execution of TestCaseCallingUserID case failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Execution of TestCaseCallingUserID case Successful" << std::endl;
    return EXIT_SUCCESS;
}
