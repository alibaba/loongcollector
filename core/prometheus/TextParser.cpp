/*
 * Copyright 2024 iLogtail Authors
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

#include "prometheus/TextParser.h"

#include <re2/re2.h>

#include <boost/algorithm/string.hpp>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <sstream>
#include <string>

#include "common/StringTools.h"
#include "logger/Logger.h"
#include "models/MetricEvent.h"

using namespace std;

namespace logtail {

// TODO: 830移除正则依赖
const std::string SAMPLE_RE = R"""(^(?P<name>\w+)(\{(?P<labels>[^}]+)\})?\s+(?P<value>\S+)(\s+(?P<timestamp>\S+))?)""";
const string JOB = "job";
const string INSTANCE = "instance";

PipelineEventGroup TextParser::Parse(const string& content) {
    auto now = std::chrono::system_clock::now();
    auto duration_since_epoch = now.time_since_epoch();
    auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(duration_since_epoch);
    std::time_t defaultTsInSecs = seconds_since_epoch.count();
    return Parse(content, defaultTsInSecs, "", "");
}

// TODO: jobName和instance在后续移动到接近业务的位置
PipelineEventGroup
TextParser::Parse(const string& content, const time_t defaultTsInSecs, const string& jobName, const string& instance) {
    string line;
    string argName, argLabels, argUnwrappedLabels, argValue, argSuffix, argTimestamp;
    istringstream iss(content);
    auto eGroup = PipelineEventGroup(make_shared<SourceBuffer>());
    while (getline(iss, line)) {
        // trim line
        line = TrimString(line);

        // skip any empty line
        if (line.empty()) {
            continue;
        }

        // skip any comment
        if (line[0] == '#') {
            continue;
        }

        // parse line
        // for given sample R"""(test_metric{k1="v1", k2="v2"} 9.9410452992e+10 1715829785083)"""
        // argName = "test_metric"
        // argLabels = R"""({"k1="v1", k2="v2"})"""
        // argUnwrappedLabels = R"""(k1="v1", k2="v2")"""
        // argValue = "9.9410452992e+10"
        // argSuffix = " 1715829785083"
        // argTimestamp = "1715829785083"
        if (RE2::FullMatch(line,
                           mSampleRegex,
                           RE2::Arg(&argName),
                           RE2::Arg(&argLabels),
                           RE2::Arg(&argUnwrappedLabels),
                           RE2::Arg(&argValue),
                           RE2::Arg(&argSuffix),
                           RE2::Arg(&argTimestamp))
            == false) {
            continue;
        }

        // skip any sample that has no name
        if (argName.empty()) {
            continue;
        }

        // skip any sample that has a NaN value
        double value = 0;
        try {
            value = stod(argValue);
        } catch (const exception&) {
            LOG_WARNING(sLogger, ("invalid value", argValue)("raw line", line));
            continue;
        }
        if (isnan(value)) {
            continue;
        }

        // set timestamp to `defaultTsInSecs` if timestamp is empty, otherwise parse it
        // if timestamp is not empty but not a valid integer, skip it
        time_t timestamp;
        if (argTimestamp.empty()) {
            timestamp = defaultTsInSecs;
        } else {
            try {
                // TODO: check if timestamp is out of window （e.g. 24h)
                timestamp = stol(argTimestamp) / 1000;
                // TODO: convert milli-second part into nano-second
            } catch (const exception&) {
                LOG_WARNING(sLogger, ("invalid value", argTimestamp)("raw line", line));
                continue;
            }
        }

        MetricEvent* e = eGroup.AddMetricEvent();
        e->SetName(argName);
        e->SetTimestamp(timestamp);
        e->SetValue<UntypedSingleValue>(value);

        if (!argUnwrappedLabels.empty()) {
            string kvPair;
            istringstream iss(argUnwrappedLabels);
            while (getline(iss, kvPair, ',')) {
                kvPair = TrimString(kvPair);

                size_t equalsPos = kvPair.find('=');
                if (equalsPos != string::npos) {
                    string key = kvPair.substr(0, equalsPos);
                    string value = kvPair.substr(equalsPos + 1);
                    value = TrimString(value, '\"', '\"');
                    e->SetTag(key, value);
                }
            }
        }
        if (!jobName.empty()) {
            e->SetTag(JOB, jobName);
        }
        if (!instance.empty()) {
            e->SetTag(INSTANCE, instance);
        }
    }

    return eGroup;
}

} // namespace logtail
