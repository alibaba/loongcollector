#include <json/json.h>

#include <chrono> // Include the <chrono> header for sleep_for
#include <thread> // Include the <thread> header for this_thread

#include "application/Application.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "controller/EventDispatcher.h"
#include "event_handler/LogInput.h"
#include "pipeline/PipelineManager.h"
#include "plugin/PluginRegistry.h"
#include "polling/PollingDirFile.h"
#include "polling/PollingEventQueue.h"
#include "polling/PollingModify.h"
#include "monitor/Monitor.h"
#include "file_server/FileServer.h"
#include "processor/daemon/LogProcess.h"
#include "unittest/Unittest.h"

using namespace std;

DECLARE_FLAG_INT32(default_max_inotify_watch_num);
DECLARE_FLAG_BOOL(enable_polling_discovery);
DECLARE_FLAG_INT32(check_timeout_interval);
DECLARE_FLAG_INT32(log_input_thread_wait_interval);
DECLARE_FLAG_INT32(check_not_exist_file_dir_round);
DECLARE_FLAG_INT32(polling_check_timeout_interval);

namespace logtail {

struct TestVector {
    string mConfigInputDir;
    string mTestDir0;
    string mTestDir1;
    int mPreservedDirDepth;
    bool mLetTimeoutBefore2ndWriteTestFile0;
    // expected results
    bool mCollectTestFile1stWrite;
    bool mCollectTestFile2ndWrite;
    bool mCollectTestFile3rdWrite;
    bool mCollectTestFile2;
};

// clang-format off
// |  用例  |  PreservedDirDepth  |  Path  |  第一次文件和目录变化  |  预期采集结果  |  第二次变化时间  |  第二次文件和目录变化  |  预期采集结果  |  第三次变化时间  |  第三次文件和目录变化  |  预期采集结果 /var/log/0/0.log  |  预期采集结果 /var/log/1/0.log  |
// | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
// |  0  |  0  |  /var/log  |  /var/log/app/0/0.log  |  采集  |  <timeout  |  在原有文件上追加数据  |  采集  |  \>timeout  |  /var/log/app/1/0.log  |  不采集  |  采集  |
// |  1  |  0  |  /var/\*/log  |  /var/app/log/0/0.log  |  采集  |  \>timeout  |  在原有文件上追加数据  |  不采集  |  \>timeout  |  /var/app/log/1/0.log  |  不采集  |  采集  |
// |  2  |  1  |  /var/log  |  /var/app/log/0/0.log  |  不采集  |  <timeout  |  在原有文件上追加数据  |  不采集  |  \>timeout  |  /var/app/log/1/0.log  |  不采集  |  不采集  |
// |  3  |  0  |  /var/log  |  /var/log/0/0.log  |  采集  |  <timeout  |  在原有文件上追加数据  |  采集  |  \>timeout  |  /var/log/1/0.log  |  不采集  |  采集  |
// |  4  |  1  |  /var/\*/log  |  /var/log/0/0.log  |  不采集  |  \>timeout  |  在原有文件上追加数据  |  不采集  |  \>timeout  |  /var/log/1/0.log  |  不采集  |  不采集  |
// |  5  |  1  |  /var/\*/log  |  /var/app/log/0/0.log  |  采集  |  \>timeout  |  在原有文件上追加数据  |  采集  |  \>timeout  |  /var/app/log/1/0.log  |  采集  |  采集  |
// |  6  |  0  |  /var/log  |  /var/log/app/0/0.log  |  采集  |  <timeout  |  在原有文件上追加数据  |  采集  |  \>timeout  |  /var/log/app/0/1/0.log  |  不采集  |  不采集  |
// clang-format on

class PollingPreservedDirDepthUnittest : public ::testing::Test {
    static std::string gRootDir;
    static std::string gCheckpoint;
    static vector<TestVector> gTestMatrix;

public:
    static void SetUpTestCase() {
        gRootDir = GetProcessExecutionDir() + "var" + PATH_SEPARATOR;
        gTestMatrix = {
            {"log", "log/app/0", "log/app/1", 0, false, true, true, false, true},
            {"*/log", "app/log/0", "app/log/1", 0, true, true, false, false, true},
            {"log", "app/log/0", "app/log/1", 1, false, false, false, false, false},
            {"log", "log/0", "log/1", 0, false, true, true, false, true},
            {"*/log", "log/0", "log/1", 1, true, false, false, false, false},
            {"*/log", "app/log/0", "app/log/1", 1, true, true, true, true, true},
            {"log", "log/app/0", "log/app/0/1", 0, false, true, true, false, false},
        };

        sLogger->set_level(spdlog::level::trace);
        srand(time(nullptr));
        INT32_FLAG(default_max_inotify_watch_num) = 0;
        BOOL_FLAG(enable_polling_discovery) = false; // we will call poll manually
        INT32_FLAG(timeout_interval) = 1;
        INT32_FLAG(check_timeout_interval) = 0;
        INT32_FLAG(check_not_exist_file_dir_round) = 1;
        INT32_FLAG(polling_check_timeout_interval) = 0;
        AppConfig::GetInstance()->mCheckPointFilePath = GetProcessExecutionDir() + gCheckpoint;
        if (bfs::exists(AppConfig::GetInstance()->mCheckPointFilePath)) {
            bfs::remove_all(AppConfig::GetInstance()->mCheckPointFilePath);
        }
        LogFileProfiler::GetInstance();
        LogtailMonitor::GetInstance()->Init();
        PluginRegistry::GetInstance()->LoadPlugins();
        LogProcess::GetInstance()->Start();
        PipelineManager::GetInstance();
        FileServer::GetInstance()->Start();
        PollingDirFile::GetInstance()->Start();
        PollingModify::GetInstance()->Start();
        PollingModify::GetInstance()->Stop();
        PollingDirFile::GetInstance()->Stop();
        PollingDirFile::GetInstance()->mRuningFlag = true;
        PollingModify::GetInstance()->mRuningFlag = true;
    }

    static void TearDownTestCase() {
        PollingDirFile::GetInstance()->mRuningFlag = false;
        PollingModify::GetInstance()->mRuningFlag = false;
        Application::GetInstance()->Exit();
    }

    void SetUp() override {
        if (bfs::exists(AppConfig::GetInstance()->mCheckPointFilePath)) {
            bfs::remove_all(AppConfig::GetInstance()->mCheckPointFilePath);
        }
        if (bfs::exists(gRootDir)) {
            bfs::remove_all(gRootDir);
        }
        bfs::create_directories(gRootDir);
    }

    void TearDown() override {
        FileServer::GetInstance()->Pause();
        for (auto& p : PipelineManager::GetInstance()->mPipelineNameEntityMap) {
            p.second->Stop(true);
        }
        PipelineManager::GetInstance()->mPipelineNameEntityMap.clear();
        // EventDispatcher::GetInstance()->CleanEnviroments();
        // ConfigManager::GetInstance()->CleanEnviroments();
        PollingDirFile::GetInstance()->ClearCache();
        PollingModify::GetInstance()->ClearCache();
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        // PollingEventQueue::GetInstance()->Clear();
        bfs::remove_all(gRootDir);
        if (bfs::exists(AppConfig::GetInstance()->mCheckPointFilePath)) {
            bfs::remove_all(AppConfig::GetInstance()->mCheckPointFilePath);
        }
        FileServer::GetInstance()->Resume();
    }

private:
    unique_ptr<Json::Value> createPipelineConfig(const string& filePath, int preservedDirDepth) {
        const char* confCstr = R"({
            "inputs": [
                {
                    "Type": "input_file",
                    "FilePaths": ["/var/log/**/0.log"],
                    "MaxDirSearchDepth": 2,
                    "PreservedDirDepth": -1
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_stdout"
                }
            ]
        })";
        unique_ptr<Json::Value> conf(new Json::Value(Json::objectValue));
        string errorMsg;
        ParseJsonTable(confCstr, *conf, errorMsg);
        auto& input = (*conf)["inputs"][0];
        input["FilePaths"][0] = filePath;
        input["PreservedDirDepth"] = preservedDirDepth;
        return conf;
    }

    void generateLog(const string& testFile) {
        LOG_DEBUG(sLogger, ("Generate log", testFile));
        auto pos = testFile.rfind(PATH_SEPARATOR);
        auto dir = testFile.substr(0, pos);
        bfs::create_directories(dir);
        ofstream ofs(testFile, std::ios::app);
        ofs << "0\n";
    }

    bool isFileDirRegistered(const string& testFile) {
        auto pos = testFile.rfind(PATH_SEPARATOR);
        auto dir = testFile.substr(0, pos);
        auto registerStatus = EventDispatcher::GetInstance()->IsDirRegistered(dir);
        return registerStatus == DirRegisterStatus::PATH_INODE_REGISTERED;
    }

    void testPollingDirFile(const TestVector& testVector) {
        auto configInputFilePath
            = gRootDir + testVector.mConfigInputDir + PATH_SEPARATOR + "**" + PATH_SEPARATOR + "0.log";
        auto testFile1 = gRootDir + testVector.mTestDir0 + PATH_SEPARATOR + "0.log";
        auto testFile2 = gRootDir + testVector.mTestDir1 + PATH_SEPARATOR + "0.log";
        FileServer::GetInstance()->Pause();
        auto configJson = createPipelineConfig(configInputFilePath, testVector.mPreservedDirDepth);
        Config pipelineConfig("polling", std::move(configJson));
        APSARA_TEST_TRUE_FATAL(pipelineConfig.Parse());
        auto p = PipelineManager::GetInstance()->BuildPipeline(
            std::move(pipelineConfig)); // reference: PipelineManager::UpdatePipelines
        APSARA_TEST_FALSE_FATAL(p.get() == nullptr);
        PipelineManager::GetInstance()->mPipelineNameEntityMap[pipelineConfig.mName] = p;
        p->Start();
        FileServer::GetInstance()->Resume();

        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        std::this_thread::sleep_for(std::chrono::microseconds(
            10 * INT32_FLAG(log_input_thread_wait_interval))); // give enough time to consume event

        // write testFile1 for the 1st time
        generateLog(testFile1);
        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        std::this_thread::sleep_for(std::chrono::microseconds(
            10 * INT32_FLAG(log_input_thread_wait_interval))); // give enough time to consume event
        if (testVector.mCollectTestFile1stWrite) {
            APSARA_TEST_TRUE_FATAL(isFileDirRegistered(testFile1));
        } else {
            APSARA_TEST_FALSE_FATAL(isFileDirRegistered(testFile1));
        }

        if (testVector.mLetTimeoutBefore2ndWriteTestFile0) {
            std::this_thread::sleep_for(std::chrono::seconds(
                2
                * INT32_FLAG(
                    timeout_interval))); // let timeout happen, must *2 since timeout happen only if time interval > 1s
        }

        // trigger clean timeout polling cache
        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        // write testFile1 for the 2nd time
        generateLog(testFile1);
        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        std::this_thread::sleep_for(std::chrono::microseconds(
            10 * INT32_FLAG(log_input_thread_wait_interval))); // give enough time to consume event
        if (testVector.mCollectTestFile2ndWrite) {
            APSARA_TEST_TRUE_FATAL(isFileDirRegistered(testFile1));
        } else {
            APSARA_TEST_FALSE_FATAL(isFileDirRegistered(testFile1));
        }

        std::this_thread::sleep_for(std::chrono::seconds(
            2
            * INT32_FLAG(
                timeout_interval))); // let timeout happen, must *2 since timeout happen only if time interval > 1s

        // trigger clean timeout polling cache
        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        generateLog(testFile1);
        generateLog(testFile2);
        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        std::this_thread::sleep_for(std::chrono::microseconds(
            10 * INT32_FLAG(log_input_thread_wait_interval))); // give enough time to consume event
        if (testVector.mCollectTestFile3rdWrite) {
            APSARA_TEST_TRUE_FATAL(isFileDirRegistered(testFile1));
        } else {
            APSARA_TEST_FALSE_FATAL(isFileDirRegistered(testFile1));
        }
        if (testVector.mCollectTestFile2) {
            APSARA_TEST_TRUE_FATAL(isFileDirRegistered(testFile2));
        } else {
            APSARA_TEST_FALSE_FATAL(isFileDirRegistered(testFile2));
        }
    }

public:
    void TestPollingDirFile0() { testPollingDirFile(gTestMatrix[0]); }
    void TestPollingDirFile1() { testPollingDirFile(gTestMatrix[1]); }
    void TestPollingDirFile2() { testPollingDirFile(gTestMatrix[2]); }
    void TestPollingDirFile3() { testPollingDirFile(gTestMatrix[3]); }
    void TestPollingDirFile4() { testPollingDirFile(gTestMatrix[4]); }
    void TestPollingDirFile5() { testPollingDirFile(gTestMatrix[5]); }

    void TestPollingDirFile6() { testPollingDirFile(gTestMatrix[6]); }

    void TestCheckpoint() {
        auto configInputFilePath = gRootDir + "log/**/0.log";
        auto testFile = gRootDir + "log/0/0.log";
        FileServer::GetInstance()->Pause();
        auto configJson = createPipelineConfig(configInputFilePath, 0);
        Config pipelineConfig("polling", std::move(configJson));
        APSARA_TEST_TRUE_FATAL(pipelineConfig.Parse());
        auto p = PipelineManager::GetInstance()->BuildPipeline(
            std::move(pipelineConfig)); // reference: PipelineManager::UpdatePipelines
        APSARA_TEST_FALSE_FATAL(p.get() == nullptr);
        PipelineManager::GetInstance()->mPipelineNameEntityMap[pipelineConfig.mName] = p;
        p->Start();
        FileServer::GetInstance()->Resume();

        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        std::this_thread::sleep_for(std::chrono::microseconds(
            10 * INT32_FLAG(log_input_thread_wait_interval))); // give enough time to consume event

        // generate log for testFile1 for the 1st time
        generateLog(testFile);
        PollingDirFile::GetInstance()->PollingIteration();
        PollingModify::GetInstance()->PollingIteration();
        std::this_thread::sleep_for(std::chrono::microseconds(
            10 * INT32_FLAG(log_input_thread_wait_interval))); // give enough time to consume event
        APSARA_TEST_TRUE_FATAL(isFileDirRegistered(testFile));

        // Dump and load checkpoint
        FileServer::GetInstance()->Pause(true);
        std::this_thread::sleep_for(std::chrono::seconds(
            2
            * INT32_FLAG(
                timeout_interval))); // let timeout happen, must *2 since timeout happen only if time interval > 1s
        FileServer::GetInstance()->Resume(true);
        // Should remain registered after checkpoint
        APSARA_TEST_TRUE_FATAL(isFileDirRegistered(testFile));

        std::this_thread::sleep_for(std::chrono::seconds(
            2
            * INT32_FLAG(
                timeout_interval))); // let timeout happen, must *2 since timeout happen only if time interval > 1s

        APSARA_TEST_FALSE_FATAL(isFileDirRegistered(testFile));
        // Dump and load checkpoint
        FileServer::GetInstance()->Pause(true);
        FileServer::GetInstance()->Resume(true);
        // Should remain unregistered after checkpoint
        APSARA_TEST_FALSE_FATAL(isFileDirRegistered(testFile));
    }
};

UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestPollingDirFile0);
UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestPollingDirFile1);
UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestPollingDirFile2);
UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestPollingDirFile3);
UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestPollingDirFile4);
UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestPollingDirFile5);
UNIT_TEST_CASE(PollingPreservedDirDepthUnittest, TestCheckpoint);

std::string PollingPreservedDirDepthUnittest::gRootDir;
std::string PollingPreservedDirDepthUnittest::gCheckpoint = "checkpoint";
vector<TestVector> PollingPreservedDirDepthUnittest::gTestMatrix;
} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}