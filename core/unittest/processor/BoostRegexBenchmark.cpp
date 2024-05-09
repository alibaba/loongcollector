// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <boost/regex.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "unittest/Unittest.h"


using namespace logtail;


std::string formatSize(long long size) {
    static const char* units[] = {" B", "KB", "MB", "GB", "TB"};
    int index = 0;
    double doubleSize = static_cast<double>(size);
    while (doubleSize >= 1024.0 && index < 4) {
        doubleSize /= 1024.0;
        index++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << std::setw(6) << std::setfill(' ') << doubleSize << " " << units[index];
    return ss.str();
}

static void BM_Regex_Match(int size, int batchSize) {
    std::string buffer = "cnt:";
    std::string regStr = "cnt.*";
    boost::regex reg(regStr);
    std::ofstream outFile("BM_Regex_Match.txt", std::ios::trunc);
    outFile.close();

    for (int i = 0; i < size; i++) {
        std::ofstream outFile("BM_Regex_Match.txt", std::ios::app);
        buffer += "a";

        int count = 0;
        uint64_t durationTime = 0;
        for (int i = 0; i < batchSize; i++) {
            count++;
            uint64_t startTime = GetCurrentTimeInMicroSeconds();
            if (!boost::regex_match(buffer, reg)) {
                std::cout << "error" << std::endl;
            }
            durationTime += GetCurrentTimeInMicroSeconds() - startTime;
        }
        outFile << i << '\t' << "durationTime: " << durationTime << std::endl;
        outFile << i << '\t' << "process: " << formatSize(buffer.size() * (uint64_t)count * 1000000 / durationTime)
                << std::endl;
        outFile.close();
    }
}

static void BM_Regex_Search(int size, int batchSize) {
    std::string buffer = "cnt:";
    std::string regStr = "^cnt";
    boost::regex reg(regStr);
    std::ofstream outFile("BM_Regex_Search.txt", std::ios::trunc);
    outFile.close();

    for (int i = 0; i < size; i++) {
        std::ofstream outFile("BM_Regex_Search.txt", std::ios::app);
        buffer += "a";

        int count = 0;
        uint64_t durationTime = 0;
        for (int i = 0; i < batchSize; i++) {
            count++;
            uint64_t startTime = GetCurrentTimeInMicroSeconds();
            if (!boost::regex_search(buffer, reg)) {
                std::cout << "error" << std::endl;
            }
            durationTime += GetCurrentTimeInMicroSeconds() - startTime;
        }

        outFile << i << '\t' << "durationTime: " << durationTime << std::endl;
        outFile << i << '\t' << "process: " << formatSize(buffer.size() * (uint64_t)count * 1000000 / durationTime)
                << std::endl;
        outFile.close();
    }
}

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
#ifdef NDEBUG
    std::cout << "release" << std::endl;
#else
    std::cout << "debug" << std::endl;
#endif
    std::cout << "BM_Regex_Match" << std::endl;
    BM_Regex_Match(100, 10000);
    std::cout << "BM_Regex_Search" << std::endl;
    BM_Regex_Search(100, 10000);
    return 0;
}