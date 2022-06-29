// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include <fstream>
#include <json/json.h>
#include "LogIntegrity.h"
#include "LogLineCount.h"
#include "util.h"

namespace logtail {

// global var
const char* gLineCountJsonFile = "logtail_line_count_snapshot.json";
const char* gIntegrityJsonFile = "logtail_integrity_snapshot.json";

class DataIntegrityUnittest : public ::testing::Test {
public:
    void TestReloadIntegrityData();
    void TestReloadLineCountData();
    void TestReloadInvalidJsonFile();

    void MockIntegrityData();
    void MockLineCountData();
    void ClearIntegrityData();
    void ClearLineCountData();
    void MockCreateIntegrityJsonFile();
    void MockCreateLineCountJsonFile();

    void DeleteTestJsonFile();

    // invalid json
    void MockCreateInvalidIntegrityJsonFile();

    // test data map
    LogIntegrity::RegionLogIntegrityInfoMap gRegionLogIntegrityInfoMap;
    LogLineCount::RegionLineCountMap gRegionLineCountMap;
};

APSARA_UNIT_TEST_CASE(DataIntegrityUnittest, TestReloadIntegrityData, 0);
APSARA_UNIT_TEST_CASE(DataIntegrityUnittest, TestReloadLineCountData, 0);
APSARA_UNIT_TEST_CASE(DataIntegrityUnittest, TestReloadInvalidJsonFile, 0);

void DataIntegrityUnittest::TestReloadIntegrityData() {
    MockIntegrityData();
    MockCreateIntegrityJsonFile();

    // reload
    LogIntegrity::GetInstance()->ReloadIntegrityDataFromLocalFile();

    // assert
    std::string region = "cn-gzone-ant";
    std::string project = "test_project";
    std::string logstore = "test_logstore";
    std::string filename = "abc.log";

    LogIntegrity::LogIntegrityInfoMap* logIntegrityInfoMap = gRegionLogIntegrityInfoMap[region];
    LogIntegrityInfo* logIntegrityInfo = (*logIntegrityInfoMap)[project + "_" + logstore + "_" + filename];

    APSARA_TEST_EQUAL(logIntegrityInfo->mIntegrityLogstore, "data_integrity");
    APSARA_TEST_EQUAL(logIntegrityInfo->mIntegrityProject, project);
    APSARA_TEST_EQUAL(logIntegrityInfo->mFilename, filename);
    APSARA_TEST_EQUAL(logIntegrityInfo->mLastUpdateTime, 1539091741);
    APSARA_TEST_EQUAL(logIntegrityInfo->mLogstore, logstore);
    APSARA_TEST_EQUAL(logIntegrityInfo->mProjectName, project);
    APSARA_TEST_EQUAL(logIntegrityInfo->mRegion, region);
    APSARA_TEST_EQUAL(logIntegrityInfo->mSendSucceededFlag, true);

    // test list
    APSARA_TEST_EQUAL(logIntegrityInfo->mLogTimeInfoList.size(), 3);
    std::list<LogTimeInfo>::const_iterator iter = logIntegrityInfo->mLogTimeInfoList.begin();
    APSARA_TEST_EQUAL(iter->mLogGroupSequenceNum, 88);
    APSARA_TEST_EQUAL(iter->mLogTime, 1539091741);
    APSARA_TEST_EQUAL(iter->mSucceededLines, 123);
    APSARA_TEST_EQUAL(iter->mStatus, 3);

    ++iter;
    APSARA_TEST_EQUAL(iter->mLogGroupSequenceNum, 89);
    APSARA_TEST_EQUAL(iter->mLogTime, 1539091744);
    APSARA_TEST_EQUAL(iter->mSucceededLines, 22);
    APSARA_TEST_EQUAL(iter->mStatus, 3);

    ++iter;
    APSARA_TEST_EQUAL(iter->mLogGroupSequenceNum, 90);
    APSARA_TEST_EQUAL(iter->mLogTime, 1539091747);
    APSARA_TEST_EQUAL(iter->mSucceededLines, 5);
    APSARA_TEST_EQUAL(iter->mStatus, 2);

    ++iter;
    APSARA_TEST_EQUAL(iter == logIntegrityInfo->mLogTimeInfoList.end(), true);

    ClearIntegrityData();
    DeleteTestJsonFile();
}

void DataIntegrityUnittest::TestReloadLineCountData() {
    MockLineCountData();
    MockCreateLineCountJsonFile();

    // reload
    LogLineCount::GetInstance()->ReloadLineCountDataFromLocalFile();

    // assert
    std::string region = "cn-gzone-ant";
    std::string project = "ant-mbaprod-test";
    std::string logstore = "rconsole-dal-digest";

    LogLineCount::mRegionLineCountMap;
    LogLineCount::LogStoreLineCountMap* lineCountMap = gRegionLineCountMap[region];
    APSARA_TEST_EQUAL(lineCountMap->size(), 2);

    LogLineCount::LogStoreLineCount* lineCount = (*lineCountMap)[project + "_" + logstore];

    APSARA_TEST_EQUAL(lineCount->mProjectName, project);
    APSARA_TEST_EQUAL(lineCount->mBizLogStore, logstore);
    APSARA_TEST_EQUAL(lineCount->mCntProjectName, project);
    APSARA_TEST_EQUAL(lineCount->mCntLogStore, "line-count");

    LogLineCount::LogStoreLineCount::LogCountPerMinuteMap& countMap = lineCount->mLogCountPerMinuteMap;
    APSARA_TEST_EQUAL(countMap.size(), 2);
    APSARA_TEST_EQUAL(countMap[1536654540], 30);
    APSARA_TEST_EQUAL(countMap[1536654600], 3);

    ClearLineCountData();
    DeleteTestJsonFile();
}

void DataIntegrityUnittest::TestReloadInvalidJsonFile() {
    MockIntegrityData();
    MockCreateInvalidIntegrityJsonFile();

    // reload
    bool result = LogIntegrity::GetInstance()->ReloadIntegrityDataFromLocalFile();

    APSARA_TEST_EQUAL(result, false);

    ClearLineCountData();
    DeleteTestJsonFile();
}

void DataIntegrityUnittest::MockIntegrityData() {
    // insert mock test data into LogProcess::mRegionLogIntegrityInfoMap
    //int64_t seqNum, time_t logTime, int32_t lines, int status
    LogTimeInfo info1(
        88, 1539091741, 123, LogTimeInfo::LogIntegrityStatus_SendOK | LogTimeInfo::LogIntegrityStatus_ParseOK);
    LogTimeInfo info2(
        89, 1539091744, 22, LogTimeInfo::LogIntegrityStatus_SendOK | LogTimeInfo::LogIntegrityStatus_ParseOK);
    LogTimeInfo info3(
        90, 1539091747, 5, LogTimeInfo::LogIntegrityStatus_SendFail | LogTimeInfo::LogIntegrityStatus_ParseOK);

    //const std::string& region, const std::string& projectName, const std::string& logstore, const std::string& filename,
    //const std::string& integrityProject, const std::string& integrityLogstore
    LogIntegrityInfo* logIntegrityInfo = new LogIntegrityInfo(
        "cn-gzone-ant", "test_project", "test_logstore", "abc.log", "aliuid", "test_project", "data_integrity");
    logIntegrityInfo->mSendSucceededFlag = true;
    logIntegrityInfo->mLastUpdateTime = 1539091741;
    logIntegrityInfo->mLogTimeInfoList.push_back(info1);
    logIntegrityInfo->mLogTimeInfoList.push_back(info2);
    logIntegrityInfo->mLogTimeInfoList.push_back(info3);

    LogIntegrity::LogIntegrityInfoMap* logIntegrityInfoMap = new LogIntegrity::LogIntegrityInfoMap;
    (*logIntegrityInfoMap)["test_project_test_logstore_abc.log"] = logIntegrityInfo;

    gRegionLogIntegrityInfoMap["cn-gzone-ant"] = logIntegrityInfoMap;
}

void DataIntegrityUnittest::MockLineCountData() {
    // insert mock test data into LogFileProfiler::mRegionLineCountMap
    LogLineCount::LogStoreLineCount* lineCount1 = new LogLineCount::LogStoreLineCount(
        "cn-gzone-ant", "ant-mbaprod-test", "rconsole-dal-digest", "ant-mbaprod-test", "line-count");
    lineCount1->mLogCountPerMinuteMap[1536654540] = 30;
    lineCount1->mLogCountPerMinuteMap[1536654600] = 3;

    LogLineCount::LogStoreLineCount* lineCount2 = new LogLineCount::LogStoreLineCount(
        "cn-gzone-ant", "ant-mbaprod-test", "startagent", "ant-mbaprod-test", "line-count");
    lineCount2->mLogCountPerMinuteMap[1536654540] = 182;
    lineCount2->mLogCountPerMinuteMap[1536654600] = 33;

    LogLineCount::LogStoreLineCountMap* lineCountMap = new LogLineCount::LogStoreLineCountMap;
    (*lineCountMap)["ant-mbaprod-test_rconsole-dal-digest"] = lineCount1;
    (*lineCountMap)["ant-mbaprod-test_startagent"] = lineCount2;

    gRegionLineCountMap["cn-gzone-ant"] = lineCountMap;
}

void DataIntegrityUnittest::ClearIntegrityData() {
    for (LogIntegrity::RegionLogIntegrityInfoMap::iterator regionIter = gRegionLogIntegrityInfoMap.begin();
         regionIter != gRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        LogIntegrity::LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;
        for (LogIntegrity::LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
             projectLogStoreFilenameIter != logIntegrityInfoMap->end();
             ++projectLogStoreFilenameIter) {
            LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;
            delete logIntegrityInfo;
        }
        logIntegrityInfoMap->clear();
        delete logIntegrityInfoMap;
    }
    gRegionLogIntegrityInfoMap.clear();
}

void DataIntegrityUnittest::ClearLineCountData() {
    for (LogLineCount::RegionLineCountMap::iterator regionIter = gRegionLineCountMap.begin();
         regionIter != gRegionLineCountMap.end();
         ++regionIter) {
        LogLineCount::LogStoreLineCountMap* lineCountMap = regionIter->second;
        for (LogLineCount::LogStoreLineCountMap::iterator projectLogstoreIter = lineCountMap->begin();
             projectLogstoreIter != lineCountMap->end();
             ++projectLogstoreIter) {
            LogLineCount::LogStoreLineCount* lineCount = projectLogstoreIter->second;
            delete lineCount;
        }
        lineCountMap->clear();
        delete lineCountMap;
    }
    gRegionLineCountMap.clear();
}

void DataIntegrityUnittest::MockCreateIntegrityJsonFile() {
    // json file
    /*
    {
       "cn-gzone-ant" : {
          "test_project_test_logstore_abc.log" : {
             "data_integrity_logstore" : "data_integrity",
             "data_integrity_project_name" : "test_project",
             "filename" : "abc.log",
             "last_update_time" : 1539091741,
             "list" : [
                 {
                     "seq_num" : 88,
                     "log_time" 1539091741: ,
                     "succeeded_lines" : 123,
                     "status" : 3
                 },
                 {
                     "seq_num" : 89,
                     "log_time" : 1539091744,
                     "succeeded_lines" : 22,
                     "status" : 3
                 },
                 {
                     "seq_num" : 90,
                     "log_time" : 1539091747,
                     "succeeded_lines" : 5,
                     "status" : 2
                 }
             ]
             "logstore" : "test_logstore",
             "project_name" : "test_project",
             "region" : "cn-gzone-ant",
             "succeeded_flag" : true,
          }
       }
    }
    */

    Json::Value root;
    // traverse log time map
    for (LogIntegrity::RegionLogIntegrityInfoMap::iterator regionIter = gRegionLogIntegrityInfoMap.begin();
         regionIter != gRegionLogIntegrityInfoMap.end();
         ++regionIter) {
        const std::string& region = regionIter->first;
        LogIntegrity::LogIntegrityInfoMap* logIntegrityInfoMap = regionIter->second;

        Json::Value regionValue;
        for (LogIntegrity::LogIntegrityInfoMap::iterator projectLogStoreFilenameIter = logIntegrityInfoMap->begin();
             projectLogStoreFilenameIter != logIntegrityInfoMap->end();
             ++projectLogStoreFilenameIter) {
            const std::string& projectLogStoreFilename = projectLogStoreFilenameIter->first;
            LogIntegrityInfo* logIntegrityInfo = projectLogStoreFilenameIter->second;

            Json::Value logTimeValue;
            logTimeValue["succeeded_flag"] = logIntegrityInfo->mSendSucceededFlag;
            logTimeValue["last_update_time"] = (Json::Int)logIntegrityInfo->mLastUpdateTime;
            logTimeValue["region"] = logIntegrityInfo->mRegion;
            logTimeValue["project_name"] = logIntegrityInfo->mProjectName;
            logTimeValue["logstore"] = logIntegrityInfo->mLogstore;
            logTimeValue["filename"] = logIntegrityInfo->mFilename;
            logTimeValue["data_integrity_project_name"] = logIntegrityInfo->mIntegrityProject;
            logTimeValue["data_integrity_logstore"] = logIntegrityInfo->mIntegrityLogstore;

            Json::Value listNode;
            LogIntegrity::SerializeListNodes(logIntegrityInfo->mLogTimeInfoList, listNode);
            logTimeValue["list"] = listNode;

            regionValue[projectLogStoreFilename.c_str()] = logTimeValue;
        }

        root[region.c_str()] = regionValue;
    }

    std::ofstream file(gIntegrityJsonFile);
    file << root.toStyledString();
    file.close();
}

void DataIntegrityUnittest::MockCreateLineCountJsonFile() {
    // json file
    /*
    {
       "cn-gzone-ant" : {
          "ant-mbaprod-test_rconsole-dal-digest" : {
             "region" : "cn-gzone-ant",
             "biz_project_name" : "ant-mbaprod-test",
             "biz_logstore" : "rconsole-dal-digest",
             "cnt_project_name" : "ant-mbaprod-test",
             "cnt_logstore" : "line-count",
             "count" : [
                {
                   "count" : 30,
                   "minute" : 1536654540
                },
                {
                   "count" : 3,
                   "minute" : 1536654600
                }
             ]
          },
          "ant-mbaprod-test_startagent" : {
             "region" : "cn-gzone-ant",
             "biz_project_name" : "ant-mbaprod-test",
             "biz_logstore" : "startagent",
             "cnt_project_name" : "ant-mbaprod-test",
             "cnt_logstore" : "line-count",
             "count" : [
                {
                   "count" : 182,
                   "minute" : 1536654540
                },
                {
                   "count" : 33,
                   "minute" : 1536654600
                }
             ]
          }
       }
    }
    */

    Json::Value root;
    // traverse line count map
    for (LogLineCount::RegionLineCountMap::iterator regionIter = gRegionLineCountMap.begin();
         regionIter != gRegionLineCountMap.end();
         ++regionIter) {
        const std::string& region = regionIter->first;
        LogLineCount::LogStoreLineCountMap* lineCountMap = regionIter->second;

        Json::Value regionValue;
        for (LogLineCount::LogStoreLineCountMap::iterator projectLogStoreIter = lineCountMap->begin();
             projectLogStoreIter != lineCountMap->end();
             ++projectLogStoreIter) {
            const std::string& projectLogStore = projectLogStoreIter->first;
            LogLineCount::LogStoreLineCount* lineCount = projectLogStoreIter->second;

            Json::Value projectLogStoreValue;
            Json::Value countValue;
            for (LogLineCount::LogStoreLineCount::LogCountPerMinuteMap::iterator minuteCountIter
                 = lineCount->mLogCountPerMinuteMap.begin();
                 minuteCountIter != lineCount->mLogCountPerMinuteMap.end();
                 ++minuteCountIter) {
                uint32_t minuteTime = minuteCountIter->first;
                uint32_t count = minuteCountIter->second;

                Json::Value minuteCountValue;
                minuteCountValue["minute"] = (Json::Int)minuteTime;
                minuteCountValue["count"] = (Json::Int)count;
                countValue.append(minuteCountValue);
            }
            projectLogStoreValue["count"] = countValue;
            projectLogStoreValue["region"] = lineCount->mRegion;
            projectLogStoreValue["biz_project_name"] = lineCount->mProjectName;
            projectLogStoreValue["biz_logstore"] = lineCount->mBizLogStore;
            projectLogStoreValue["cnt_project_name"] = lineCount->mCntProjectName;
            projectLogStoreValue["cnt_logstore"] = lineCount->mCntLogStore;

            regionValue[projectLogStore.c_str()] = projectLogStoreValue;
        }
        root[region.c_str()] = regionValue;
    }

    std::ofstream file(gLineCountJsonFile);
    file << root.toStyledString();
    file.close();
}

void DataIntegrityUnittest::DeleteTestJsonFile() {
    // delete test json file
    bfs::remove(gLineCountJsonFile);
    bfs::remove(gIntegrityJsonFile);
}

void DataIntegrityUnittest::MockCreateInvalidIntegrityJsonFile() {
    // invalid json file, missing '}' at the end
    /*
    {
       "cn-gzone-ant" : {
          "test_project_test_logstore_abc.log" : {
             "data_integrity_logstore" : "data_integrity",
             "data_integrity_project_name" : "test_project",
             "filename" : "abc.log",
             "last_update_time" : 1539091741,
             "list" : [
                 {
                     "seq_num" : 88,
                     "log_time" 1539091741: ,
                     "succeeded_lines" : 123,
                     "status" : 3
                 },
                 {
                     "seq_num" : 89,
                     "log_time" : 1539091744,
                     "succeeded_lines" : 22,
                     "status" : 3
                 },
                 {
                     "seq_num" : 90,
                     "log_time" : 1539091747,
                     "succeeded_lines" : 5,
                     "status" : 2
                 }
             ]
             "logstore" : "test_logstore",
             "project_name" : "test_project",
             "region" : "cn-gzone-ant",
             "succeeded_flag" : true,
          }
       }
    */

    MockCreateIntegrityJsonFile();
    // modify file, remove last row
    std::vector<std::string> lines;
    {
        std::ifstream in(gIntegrityJsonFile);
        std::string line;
        while (std::getline(in, line))
            lines.push_back(line);
    }
    std::ofstream out(gIntegrityJsonFile);
    for (int i = 0; i < static_cast<int>(lines.size()) - 1; ++i) {
        out << lines[i] << "\n";
    }
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}