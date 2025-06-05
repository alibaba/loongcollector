// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>
#include <filesystem>
#include <fstream>

#include "apm/PackageManager.h"
#include "apm/Types.h"
#include "unittest/Unittest.h"

namespace logtail::apm {
namespace fs = std::filesystem;

class PackageManagerUnittest : public testing::Test {
public:
    void SetUp() override {
        mPackageManager = std::make_unique<PackageManager>();
        mPackageManager->Init();
        
        mTestDir = std::filesystem::temp_directory_path() / "ilogtail_test";
        std::filesystem::create_directories(mTestDir);
        
        // 创建测试用的 ld.so.preload 文件
        mPreloadFile = mTestDir / "ld.so.preload";
        std::ofstream preloadFile(mPreloadFile);
        preloadFile.close();
    }

    void TearDown() override {
        // 清理临时测试目录
        std::filesystem::remove_all(mTestDir);
        mPackageManager.reset();
    }

    void TestPrepareExecHook() {
        // 测试安装 exec hook
        bool result = mPackageManager->PrepareExecHook("cn-heyuan");
        APSARA_TEST_TRUE(result);


        APSARA_TEST_TRUE(std::filesystem::exists("/lib64/libexec-hook.so"));

        // manifest
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/lib/exec-hook/.libexec-hook.so.manifest"));
    }

    void TestPrepareJavaAgent() {
        // 测试安装 exec hook
        fs::path agentPath;
        bool result = mPackageManager->PrepareAPMAgent(APMLanguage::kJava, "12@13", "cn-heyuan", "", agentPath);
        APSARA_TEST_TRUE(result);


        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/latest/AliyunJavaAgent.zip"));
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/current/AliyunJavaAgent.zip"));
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/current/AliyunJavaAgent/aliyun-java-agent.jar"));
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/current/AliyunJavaAgent/version"));

        // manifest
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/latest/.AliyunJavaAgent.zip.manifest"));
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/latest/.AliyunJavaAgent.zip.manifest"));
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/latest/AliyunJavaAgent/aliyun-java-agent.jar"));
        APSARA_TEST_TRUE(std::filesystem::exists("/opt/.arms/apm-java-agent/12@13/latest/AliyunJavaAgent/version"));
    }

    void TestInstallExecHook() {
        // 测试安装 exec hook
        bool result = mPackageManager->InstallExecHook("cn-heyuan");
        APSARA_TEST_TRUE(result);
        
        // 验证 ld.so.preload 文件是否被正确更新
        std::ifstream preloadFile("/etc/ld.so.preload");
        std::string content((std::istreambuf_iterator<char>(preloadFile)),
                            std::istreambuf_iterator<char>());
        APSARA_TEST_TRUE(content.find("libexec-hook.so") != std::string::npos);
    }

    void UpdateExecHookTest() {
        // // 先安装
        // bool installResult = mPackageManager->InstallExecHook("cn-heyuan");
        // APSARA_TEST_TRUE(installResult);
        
        // // 测试更新
        // bool updateResult = mPackageManager->UpdateExecHook();
        // APSARA_TEST_TRUE(updateResult);
    }

    void UninstallExecHookTest() {
        // // 先安装
        // bool installResult = mPackageManager->InstallExecHook("cn-heyuan");
        // APSARA_TEST_TRUE(installResult);
        
        // // 测试卸载
        // bool uninstallResult = mPackageManager->UninstallExecHook();
        // APSARA_TEST_TRUE(uninstallResult);
    }

    void PrepareAPMAgentTest() {
        // 测试准备 APM Agent
        fs::path agentPath;
        bool result = mPackageManager->PrepareAPMAgent(APMLanguage::kJava, "10", "cn-heyuan", "1.0.0", agentPath);
        APSARA_TEST_TRUE(result);
    }

    void PrepareAPMAgentWithInvalidRegion() {
        // 测试使用无效的区域
        fs::path agentPath;
        bool result = mPackageManager->PrepareAPMAgent(APMLanguage::kJava, "11", "invalid-region", "1.0.0", agentPath);
        APSARA_TEST_FALSE(result);
    }

    void PrepareAPMAgentWithInvalidVersion() {
        // 测试使用无效的版本
        fs::path agentPath;
        bool result = mPackageManager->PrepareAPMAgent(APMLanguage::kJava, "12", "cn-heyuan", "", agentPath);
        APSARA_TEST_FALSE(result);
    }

    void PrepareAPMAgentWithUnsupportedLanguage() {
        // 测试不支持的语言
        fs::path agentPath;
        bool result = mPackageManager->PrepareAPMAgent(APMLanguage::kPython, "13", "cn-heyuan", "1.0.0", agentPath);
        APSARA_TEST_FALSE(result);
    }

    void InstallExecHookWithExistingPreload() {
        // 准备已有的 ld.so.preload 文件内容
        std::ofstream preloadFile(mPreloadFile);
        preloadFile << "/usr/lib/other.so\n";
        preloadFile.close();
        
        // 测试安装
        bool result = mPackageManager->InstallExecHook("cn-heyuan");
        APSARA_TEST_TRUE(result);
        
        // 验证文件内容是否正确追加
        std::ifstream updatedPreloadFile(mPreloadFile);
        std::string content((std::istreambuf_iterator<char>(updatedPreloadFile)),
                            std::istreambuf_iterator<char>());
        APSARA_TEST_TRUE(content.find("/usr/lib/other.so") != std::string::npos);
        APSARA_TEST_TRUE(content.find("libexec-hook.so") != std::string::npos);
    }

    void InstallExecHookWithNoPermission() {
        // 设置文件为只读
        std::filesystem::permissions(mPreloadFile, 
                                std::filesystem::perms::owner_read,
                                std::filesystem::perm_options::replace);
        
        // 测试安装
        APSARA_TEST_FALSE(mPackageManager->InstallExecHook("cn-heyuan"));
        
        // 恢复权限
        std::filesystem::permissions(mPreloadFile, 
                                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                std::filesystem::perm_options::replace);
    }

private:
    std::unique_ptr<PackageManager> mPackageManager;
    std::filesystem::path mTestDir;
    std::filesystem::path mPreloadFile;
};

UNIT_TEST_CASE(PackageManagerUnittest, TestPrepareExecHook);
UNIT_TEST_CASE(PackageManagerUnittest, TestPrepareJavaAgent);
UNIT_TEST_CASE(PackageManagerUnittest, TestInstallExecHook);
// UNIT_TEST_CASE(PackageManagerUnittest, UpdateExecHookTest);
// UNIT_TEST_CASE(PackageManagerUnittest, UninstallExecHookTest);
// UNIT_TEST_CASE(PackageManagerUnittest, PrepareAPMAgentTest);
// UNIT_TEST_CASE(PackageManagerUnittest, PrepareAPMAgentWithInvalidRegion);
// UNIT_TEST_CASE(PackageManagerUnittest, PrepareAPMAgentWithInvalidVersion);
// UNIT_TEST_CASE(PackageManagerUnittest, PrepareAPMAgentWithUnsupportedLanguage);
// UNIT_TEST_CASE(PackageManagerUnittest, InstallExecHookWithExistingPreload);
// UNIT_TEST_CASE(PackageManagerUnittest, InstallExecHookWithNoPermission);

} // namespace logtail::apm


UNIT_TEST_MAIN
