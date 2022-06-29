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

#include "TimeUtil.h"
#include <memory.h>
#include <chrono>
#include <limits>
#include <atomic>
#if defined(__linux__)
#include <sys/sysinfo.h>
#include <utmp.h>
#endif
#include "logger/Logger.h"

namespace logtail {

const std::string PRECISE_TIMESTAMP_DEFAULT_KEY = "precise_timestamp";

std::string ConvertToTimeStamp(const time_t& t, const std::string& format) {
    return GetTimeStamp(t, format);
}

std::string GetTimeStamp(time_t t, const std::string& format) {
    if (0 == strcmp(format.c_str(), "%s")) {
        return t <= 0 ? "" : std::to_string(t);
    }

    struct tm timeInfo;
#if defined(__linux__)
    if (NULL == localtime_r(&t, &timeInfo))
        return "";
#elif defined(_MSC_VER)
    if (0 != localtime_s(&timeInfo, &t))
        return "";
#endif
    char buf[256];
    auto ret = strftime(buf, 256, format.c_str(), &timeInfo);
    return (0 == ret) ? "" : std::string(buf, ret);
}

uint64_t GetCurrentTimeInMicroSeconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

uint64_t GetCurrentTimeInMilliSeconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int GetLocalTimeZoneOffsetSecond() {
    time_t nowTime = time(NULL);
    tm timeInfo;
    memset(&timeInfo, 0, sizeof(timeInfo));
#if defined(__linux__)
    localtime_r(&nowTime, &timeInfo);
    return timeInfo.tm_gmtoff;
#else
    localtime_s(&timeInfo, &nowTime);
    char timezone[50];
    auto len = strftime(timezone, sizeof(timezone), "%z", &timeInfo); // +0800
    if (len <= 1)
        return 0;
    int sign = ('+' == timezone[0]) ? 1 : -1;
    int num = atoi(&timezone[1]);
    return sign * ((num % 100 == 0) ? (num / 100) * 3600 : num * 60);
#endif
}

// DeduceYear decuces year for @tm according to current date (@currentTm).
// @return -1 means that it can not give a deduction.
int DeduceYear(const struct tm* tm, const struct tm* currentTm) {
    // Deduction rules.
    // 1. Input date is Jan 1, current date is Dec 31, increase year.
    if ((0 == tm->tm_mon && 1 == tm->tm_mday) && (11 == currentTm->tm_mon && 31 == currentTm->tm_mday)) {
        return currentTm->tm_year + 1;
    }
    // 2. Input date is Dec 31, current date is Jan 1, descrease year.
    if ((11 == tm->tm_mon && 31 == tm->tm_mday) && (0 == currentTm->tm_mon && 1 == currentTm->tm_mday)) {
        return currentTm->tm_year - 1;
    }
    // 3. Assign directly.
    return currentTm->tm_year;
}

const char* Strptime(const char* buf, const char* fmt, struct tm* tm, int32_t specifiedYear /* = -1 */) {
    if (specifiedYear < 0)
        return strptime(buf, fmt, tm);

    const int32_t MIN_YEAR = std::numeric_limits<decltype(tm->tm_year)>::min();
    auto bakYear = tm->tm_year;
    tm->tm_year = MIN_YEAR;

    // Do not specify: already got year information.
    auto ret = strptime(buf, fmt, tm);
    if (tm->tm_year != MIN_YEAR)
        return ret;

    // Mode 1.
    if (specifiedYear > 0) {
        tm->tm_year = specifiedYear - 1900;
        return ret;
    }

    // Mode 2: deduce year according to current time.
    tm->tm_year = bakYear;
    struct tm currentTm = {0};
    time_t currentTime = time(0);
#if defined(_MSC_VER)
    if (localtime_s(&currentTm, &currentTime) != 0)
#elif defined(__linux__)
    if (NULL == localtime_r(&currentTime, &currentTm))
#endif
    {
        LOG_WARNING(sLogger, ("Call localtime failed, errno", errno));
        return ret;
    }
    auto deduction = DeduceYear(tm, &currentTm);
    if (deduction != -1)
        tm->tm_year = deduction;
    return ret;
}

#if defined(__linux__)
int ReadUtmp(const char* filename, int* n_entries, utmp** utmp_buf) {
    FILE* utmp_file;
    struct stat file_stats;
    size_t n_read;
    size_t size;
    struct utmp* buf;

    utmp_file = fopen(filename, "r");
    if (utmp_file == NULL)
        return 1;

    if (fstat(fileno(utmp_file), &file_stats) != 0)
        return 1;

    size = file_stats.st_size;
    if (size > 0)
        buf = (utmp*)malloc(size);
    else {
        fclose(utmp_file);
        return 1;
    }

    /* Use < instead of != in case the utmp just grew.  */
    n_read = fread(buf, 1, size, utmp_file);
    if (ferror(utmp_file) || fclose(utmp_file) == EOF || n_read < size) {
        free(buf);
        return 1;
    }

    *n_entries = size / sizeof(utmp);
    *utmp_buf = buf;

    return 0;
}

int32_t GetBootTimeFromUtmp() {
    int32_t bootTime = -1;
    int n_users = 0;
    utmp* utmp_buf = NULL;

    do {
        int fail = ReadUtmp("/var/run/utmp", &n_users, &utmp_buf);
        if (fail) {
            APSARA_LOG_ERROR(sLogger, ("failed to read utmp file", ""));
            break;
        }

        utmp* head = utmp_buf;
        while (n_users-- > 0) {
            if (head->ut_type == RUN_LVL) {
                bootTime = head->ut_tv.tv_sec;
                break;
            }
            ++head;
        }
    } while (0);

    if (utmp_buf != NULL)
        free(utmp_buf);

    return bootTime;
}

int32_t GetUpTime() {
    time_t uptime = 0;
    FILE* fp = fopen("/proc/uptime", "r");
    if (fp != NULL) {
        const int sz = 8192;
        char buf[sz];
        double upsecs;
        int res;
        char* b = fgets(buf, sz, fp);
        if (b == buf) {
            /* The following sscanf must use the C locale.  */
            setlocale(LC_NUMERIC, "C");
            res = sscanf(buf, "%lf", &upsecs);
            setlocale(LC_NUMERIC, "");
            if (res == 1) {
                uptime = (time_t)upsecs;
            } else {
                APSARA_LOG_ERROR(sLogger, ("failed to /proc/uptime", ""));
            }
        }
        fclose(fp);
    }
    return uptime;
}
#endif

int32_t GetSystemBootTime() {
#if defined(__linux__)
    int32_t bootTime = 0;
    int32_t upTime = GetUpTime();
    if (upTime > 0) {
        time_t currentTime = time(NULL);
        bootTime = currentTime - upTime;
        APSARA_LOG_INFO(sLogger, ("get system boot time from /proc/uptime", bootTime));
        return bootTime;
    }

    int32_t utmpBootTime = GetBootTimeFromUtmp();
    if (utmpBootTime > 0) {
        APSARA_LOG_INFO(sLogger, ("get system boot time from /var/run/utmp", utmpBootTime));
        return utmpBootTime;
    }
    return 0; // unexpected
#elif defined(_MSC_VER)
    // TODO:
    return -1;
#endif
}

static std::atomic<int32_t> sTimeDelta{0};

time_t GetTimeDelta() {
    return sTimeDelta;
}

void UpdateTimeDelta(time_t serverTime) {
    sTimeDelta = serverTime - time(0);
    APSARA_LOG_INFO(sLogger, ("update time delta by server time", serverTime)("delta", sTimeDelta));
}

// Parse ms/us/ns suffix from preciseTimeSuffix, joining with the input second timestamp.
// Will return value the precise timestamp.
uint64_t GetPreciseTimestamp(uint64_t secondTimestamp,
                             const char* preciseTimeSuffix,
                             const PreciseTimestampConfig& preciseTimestampConfig) {
    uint64_t adjustSecondTimestamp = secondTimestamp - preciseTimestampConfig.tzOffsetSecond;
    if (!preciseTimestampConfig.enabled) {
        return adjustSecondTimestamp;
    }

    bool endFlag = false;
    TimeStampUnit timeUnit = preciseTimestampConfig.unit;

    uint32_t maxPreciseDigitNum = 0;
    if (TimeStampUnit::MILLISECOND == timeUnit) {
        maxPreciseDigitNum = 3;
    } else if (TimeStampUnit::MICROSECOND == timeUnit) {
        maxPreciseDigitNum = 6;
    } else if (TimeStampUnit::NANOSECOND == timeUnit) {
        maxPreciseDigitNum = 9;
    } else {
        maxPreciseDigitNum = 0;
    }

    if (NULL == preciseTimeSuffix || strlen(preciseTimeSuffix) <= 1) {
        endFlag = true;
    } else {
        std::string supprotSeparators = ".,: ";
        const char separator = preciseTimeSuffix[0];
        std::size_t found = supprotSeparators.find(separator);
        if (found == std::string::npos) {
            endFlag = true;
        }
    }

    uint32_t preciseTimeDigit = 0;
    for (uint32_t i = 0; i < maxPreciseDigitNum; i++) {
        bool validDigit = false;
        if (!endFlag) {
            const char digitChar = preciseTimeSuffix[i + 1];
            if (digitChar != '\0' && digitChar >= '0' && digitChar <= '9') {
                preciseTimeDigit = preciseTimeDigit * 10 + (digitChar - '0');
                validDigit = true;
            }
        }

        if (!validDigit) {
            // Insufficient digits filled with zeros.
            preciseTimeDigit = preciseTimeDigit * 10;
            endFlag = true;
        }
        adjustSecondTimestamp *= 10;
    }

    return adjustSecondTimestamp + preciseTimeDigit;
}

} // namespace logtail
