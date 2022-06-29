/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>
#include <memory>
#include <boost/regex.hpp>

namespace logtail {
// config for integrity function
struct IntegrityConfig {
    std::string mAliuid;
    bool mIntegritySwitch;
    std::string mIntegrityProjectName;
    std::string mIntegrityLogstore;
    std::string mLogTimeReg; // regex format for time in log
    std::string mTimeFormat;
    boost::regex* mCompiledTimeReg;
    int32_t mTimePos;

    IntegrityConfig(const std::string& aliuid,
                    bool integritySwitch,
                    const std::string& integrityProject,
                    const std::string& integrityLogstore,
                    const std::string& timeReg,
                    const std::string& timeFormat,
                    int32_t timePos)
        : mAliuid(aliuid),
          mIntegritySwitch(integritySwitch),
          mIntegrityProjectName(integrityProject),
          mIntegrityLogstore(integrityLogstore),
          mLogTimeReg(timeReg),
          mTimeFormat(timeFormat),
          mTimePos(timePos) {
        mCompiledTimeReg = new boost::regex(timeReg.c_str());
    }

    IntegrityConfig(const IntegrityConfig& cfg)
        : mAliuid(cfg.mAliuid),
          mIntegritySwitch(cfg.mIntegritySwitch),
          mIntegrityProjectName(cfg.mIntegrityProjectName),
          mIntegrityLogstore(cfg.mIntegrityLogstore),
          mLogTimeReg(cfg.mLogTimeReg),
          mTimeFormat(cfg.mTimeFormat),
          mTimePos(cfg.mTimePos) {
        mCompiledTimeReg = new boost::regex(cfg.mLogTimeReg.c_str());
    }

    IntegrityConfig& operator=(const IntegrityConfig& cfg) {
        if (this != &cfg) {
            mAliuid = cfg.mAliuid;
            mIntegritySwitch = cfg.mIntegritySwitch;
            mIntegrityProjectName = cfg.mIntegrityProjectName;
            mIntegrityLogstore = cfg.mIntegrityLogstore;
            mLogTimeReg = cfg.mLogTimeReg;
            mTimeFormat = cfg.mTimeFormat;
            mCompiledTimeReg = new boost::regex(cfg.mLogTimeReg.c_str());
            mTimePos = cfg.mTimePos;
        }

        return *this;
    }

    ~IntegrityConfig() { delete mCompiledTimeReg; }
};

struct LineCountConfig {
    std::string mAliuid;
    bool mLineCountSwitch;
    std::string mLineCountProjectName;
    std::string mLineCountLogstore;

    LineCountConfig(const std::string& aliuid,
                    bool lineCountSwitch,
                    const std::string& lineCountProject,
                    const std::string& lineCountLogstore)
        : mAliuid(aliuid),
          mLineCountSwitch(lineCountSwitch),
          mLineCountProjectName(lineCountProject),
          mLineCountLogstore(lineCountLogstore) {}
};

typedef std::shared_ptr<IntegrityConfig> IntegrityConfigPtr;
typedef std::shared_ptr<LineCountConfig> LineCountConfigPtr;

} // namespace logtail
