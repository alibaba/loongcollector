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
#include <ctime>
#include <thread>

// Time and timestamp utility.
namespace logtail {

extern const std::string PRECISE_TIMESTAMP_DEFAULT_KEY;

enum class TimeStampUnit { SECOND, MILLISECOND, MICROSECOND, NANOSECOND };

struct PreciseTimestampConfig {
    bool enabled = false;
    std::string key = PRECISE_TIMESTAMP_DEFAULT_KEY;
    TimeStampUnit unit = TimeStampUnit::MILLISECOND;

    int tzOffsetSecond = 0;
};

// Convert @tm to string accroding to @format. TODO: Merge ConvertToTimeStamp and GetTimeStamp.
std::string ConvertToTimeStamp(const time_t& tm, const std::string& format = "%Y%m%d%H%M%S");
std::string GetTimeStamp(time_t tm, const std::string& format = "%Y%m%d%H%M%S");

// Get current time in us or ms.
uint64_t GetCurrentTimeInMicroSeconds();
uint64_t GetCurrentTimeInMilliSeconds();

// Get offset between current time zone and UTC in seconds.
// For example, for UTC+8, returns 8*60*60.
int GetLocalTimeZoneOffsetSecond();

#if defined(_MSC_VER)
inline void usleep(uint64_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void sleep(uint64_t s) {
    std::this_thread::sleep_for(std::chrono::seconds(s));
}
#endif

// Strptime works like standard Unix strptime, but if there is no year information
// in @buf and @fmt, you can set @specifiedYear to fill tm->tm_year.
//
// @specifiedYear: set to negative (<0) means do nothing. It offers two modes:
//   1. You can specify a fixed positive year (such as 2018). This mode is suitable
//     for collecting history logs, such as logs generated last year.
//   2. You can set to 0 to ask Strptime to make a deduction according to current date.
//     This mode only suits for real-time logs which lack of year information, for
//     example, syslog following RFC3164 does not generate year information.
const char* Strptime(const char* buf, const char* fmt, struct tm* tm, int32_t specifiedYear = -1);

int32_t GetSystemBootTime();

// For feature enable_log_time_auto_adjust.
time_t GetTimeDelta();
void UpdateTimeDelta(time_t serverTime);

uint64_t GetPreciseTimestamp(uint64_t secondTimestamp,
                             const char* preciseTimeSuffix,
                             const PreciseTimestampConfig& preciseTimestampConfig);
} // namespace logtail

#if defined(_MSC_VER)
extern "C" const char* strptime(const char* buf, const char* fmt, struct tm* tm);
#endif
