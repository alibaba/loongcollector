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

#include <iostream>
#include <sstream>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include "json/json.h"

using namespace std;

#ifdef ENABLE_COMPATIBLE_MODE
extern "C" {
#include <string.h>
asm(".symver memcpy, memcpy@GLIBC_2.2.5");
void* __wrap_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}
}
#endif

string LogFile = "";
string LogBeginReg = "";
string LogRegex = "";

void checkRegex(const string& logFile, const string& logBeginReg, const string& logRegex);
vector<string> checkLogBeginReg(const string& logFile, const string& logBeginReg, const bool display);
void checkFilterRegex(const string& logFile, const string& logRegex);

int main(int argc, char* argv[]) //argv is logFile,logBeginReg,logRegex
{
    if (3 == argc) {
        if (0 == strcmp(argv[1], "-f")) {
            FILE* pFile = fopen(argv[2], "r");
            if (pFile != NULL) {
                size_t MAXBUF = 10 * 1024 * 1024;
                char* buffer = (char*)malloc(MAXBUF + 1);
                fgets(buffer, MAXBUF, pFile);
                LogBeginReg = buffer;
                if (LogBeginReg.size() > 0 && LogBeginReg[LogBeginReg.size() - 1] == '\n') {
                    LogBeginReg.resize(LogBeginReg.size() - 1);
                }
                fgets(buffer, MAXBUF, pFile);
                LogRegex = buffer;

                if (LogRegex.size() > 0 && LogRegex[LogRegex.size() - 1] == '\n') {
                    LogRegex.resize(LogRegex.size() - 1);
                }
                fgets(buffer, MAXBUF, pFile);
                LogFile = buffer;

                if (LogFile.size() > 0 && LogFile[LogFile.size() - 1] == '\n') {
                    LogFile.resize(LogFile.size() - 1);
                }
                checkRegex(LogFile, LogBeginReg, LogRegex);
                free(buffer);
                fclose(pFile);
            }
        } else {
            LogFile = argv[1];
            LogBeginReg = argv[2];
            checkLogBeginReg(LogFile, LogBeginReg, true);
        }
    } else if (4 == argc) {
        if (0 == strcmp(argv[3], "filter")) {
            LogFile = argv[1];
            LogRegex = argv[2];
            checkFilterRegex(LogFile, LogRegex);
        } else {
            LogFile = argv[1];
            LogBeginReg = argv[2];
            LogRegex = argv[3];
            checkRegex(LogFile, LogBeginReg, LogRegex);
        }
    } else {
        Json::Value jsonResult;
        Json::FastWriter jsonWriter;
        jsonResult["result"]["state"] = false;
        jsonResult["result"]["content"] = "argument's format is not correct";
        cout << jsonWriter.write(jsonResult);
    }
    return 0;
}
void checkFilterRegex(const string& logFile, const string& logRegex) {
    Json::Value jsonResult;
    Json::Value jsonRoot;
    Json::FastWriter jsonWriter;
    try {
        boost::regex reg(logRegex);
        boost::match_results<const char*> what;
        if (0 == boost::regex_match(logFile.c_str(), what, reg, boost::match_default)) {
            jsonRoot["state"] = "false";
            jsonRoot["content"] = Json::Value();
        } else {
            jsonRoot["state"] = "true";
            jsonRoot["content"] = what.str();
        }
        jsonResult["result"] = jsonRoot;
        cout << jsonWriter.write(jsonResult);
    } catch (boost::regex_error& e) {
        jsonRoot["state"] = "error";
        jsonRoot["content"] = "过滤正则表达式错误!";
        jsonResult["result"] = jsonRoot;
        cout << jsonWriter.write(jsonResult);
        return;
    }
}

void checkRegex(const string& logFile, const string& logBeginReg, const string& logRegex) {
    Json::Value jsonResult;
    Json::Value jsonRoot;
    Json::FastWriter jsonWriter;
    try {
        boost::regex reg(logRegex);
        boost::match_results<const char*> what;
        string logFileTmp = logFile;
        if ("" != logBeginReg) {
            vector<string> logFileVector = checkLogBeginReg(logFile, logBeginReg, false);
            logFileTmp = logFileVector[0];
            boost::replace_first(logFileTmp, "\n", "");
        }
        string keyPrefix = "key_";
        if (0 == boost::regex_match(logFileTmp.c_str(), what, reg, boost::match_default)) {
            jsonRoot["state"] = "false";
            jsonRoot["content"] = Json::Value();
        } else {
            jsonRoot["state"] = "true";
            Json::Value content;
            stringstream ss;
            for (size_t i = 0; i < what.size(); i++) {
                ss << i;
                string keyName = keyPrefix + ss.str();
                content[keyName] = what.str(i);
                ss.str("");
            }
            jsonRoot["content"] = content;
        }
        jsonResult["result"] = jsonRoot;
        cout << jsonWriter.write(jsonResult);
    } catch (boost::regex_error& e) {
        jsonRoot["state"] = "error";
        jsonRoot["content"] = "日志内容正则表达式错误!";
        jsonResult["result"] = jsonRoot;
        cout << jsonWriter.write(jsonResult);
        return;
    }
}

vector<string> checkLogBeginReg(const string& logFile, const string& logBeginReg, const bool display) {
    Json::FastWriter jsonWriter;
    Json::Value jsonRoot;
    Json::Value jsonResult;
    Json::Value jsonPosition;
    vector<string> resultVector;
    vector<int> positionVector;
    vector<string> outputVector;
    boost::split(resultVector, logFile, boost::is_any_of("\n"));

    try {
        boost::regex pattern(logBeginReg);
        size_t index = 0;
        for (index = 0; index < resultVector.size(); index++) {
            boost::match_results<const char*> result;
            if (0 != boost::regex_match(resultVector[index].c_str(), result, pattern, boost::match_default)) {
                if (result.str(0).length() == resultVector[index].length()) {
                    positionVector.push_back(index);
                }
            }
            jsonResult.append(resultVector[index]);
        }
        string outputRes = "";
        for (index = 0; index < positionVector.size(); index++) {
            size_t logRow = 0;
            if (index == (positionVector.size() - 1)) {
                for (logRow = positionVector[index]; logRow < resultVector.size(); logRow++) {
                    outputRes += resultVector[logRow] + "\n";
                }
            } else {
                for (logRow = positionVector[index]; logRow < (unsigned)positionVector[index + 1]; logRow++) {
                    outputRes += resultVector[logRow] + "\n";
                }
            }
            outputVector.push_back(outputRes);
            jsonPosition.append(positionVector[index]);
        }
        jsonRoot["result"]["state"] = "";
        jsonRoot["result"]["result"] = jsonResult;
        jsonRoot["result"]["position"] = jsonPosition;

        if (display)
            cout << jsonWriter.write(jsonRoot);
        return outputVector;
    } catch (boost::regex_error& e) {
        jsonRoot["state"] = "error";
        jsonRoot["content"] = "日志起始匹配正则表达式错误!";
        jsonResult["result"] = jsonRoot;
        cout << jsonWriter.write(jsonResult);
        return resultVector;
    }
}
