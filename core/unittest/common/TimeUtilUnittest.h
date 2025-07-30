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

#include <string>
#include <vector>

#include "common/StringTools.h"
#include "common/Strptime.h"
#include "common/TimeUtil.h"
#include "unittest/Unittest.h"

namespace logtail {

int DeduceYear(const struct tm* tm, const struct tm* currentTm);

class TimeUtilUnittest : public ::testing::Test {
public:
    void TestDeduceYear();
    void TestStrptime();
    void TestNativeStrptimeFormat();
    void TestStrptimeNanosecond();
    void TestGetPreciseTimestampFromLogtailTime();
    void TestBootTimeDiff();
    void TestKernelTimeToUTC();
};

APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestDeduceYear, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestStrptime, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestNativeStrptimeFormat, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestStrptimeNanosecond, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestGetPreciseTimestampFromLogtailTime, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestBootTimeDiff, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestKernelTimeToUTC, 0);

void TimeUtilUnittest::TestBootTimeDiff() {
    auto diff = GetTimeDiffFromMonotonic();
    uint64_t num = static_cast<uint64_t>(diff.count());
    APSARA_TEST_GE(num, (uint64_t)0);
}

void TimeUtilUnittest::TestKernelTimeToUTC() {
    auto ts = ConvertKernelTimeToUnixTime(1000);
    APSARA_TEST_GE(ts.tv_nsec, 1000);
}

void TimeUtilUnittest::TestDeduceYear() {
    struct Case {
        std::string input;
        std::string current;
        int expected;
    };
    auto parse = [](const std::string& s, struct tm* t) {
        auto sp = SplitString(s, "/");
        StringTo(sp[0], t->tm_year);
        t->tm_year -= 1900;
        StringTo(sp[1], t->tm_mon);
        t->tm_mon -= 1;
        StringTo(sp[2], t->tm_mday);
    };

    std::vector<Case> cases{{"0/12/31", "2018/1/1", 2017},
                            {"0/1/1", "2018/12/31", 2019},
                            {"0/2/28", "2018/3/1", 2018},
                            {"0/2/29", "2016/2/29", 2016},
                            // We can not deduce for history logs.
                            {"0/9/9", "2018/1/1", 2018}};

    for (auto& c : cases) {
        struct tm input {};
        struct tm current {};
        parse(c.input, &input);
        parse(c.current, &current);
        EXPECT_EQ(c.expected, 1900 + DeduceYear(&input, &current));
    }
}

void TimeUtilUnittest::TestStrptime() {
#ifdef __linux__
    // Only linux supports native strptime.
    struct Case {
        std::string buf;
        std::string format;
        int specifiedYear;
        int expectedYear;
    };

    // Because we use current year, this test might fail if it is runned at the last
    // day of a year.
    time_t currentTime = time(0);
    auto currentYear = localtime(&currentTime)->tm_year;
    std::vector<Case> cases{
        {"2012/01/01", "%Y/%M/%d", -1, 2012},
        {"2011/01/01", "%Y/%M/%d", 2018, 2011},
        {"01/01", "%M/%d", 2013, 2013},
        {"2010/01/01", "%Y/%M/%d", 0, 2010},
        {"01/02", "%M/%d", 0, currentYear + 1900},
        {"1900/01/01", "%Y/%M/%d", 2018, 1900},
        {"1562925089", "%s", 0, 2019},
    };

    for (auto& c : cases) {
        struct tm o1 = {0};
        LogtailTime o2;
        int nanosecondLength;
        auto ret1 = strptime(c.buf.c_str(), c.format.c_str(), &o1);
        auto ret2 = Strptime(c.buf.c_str(), c.format.c_str(), &o2, nanosecondLength, c.specifiedYear);

        EXPECT_TRUE(ret1 != NULL);
        EXPECT_TRUE(ret2 != NULL);
        o1.tm_year = c.expectedYear - 1900;
        EXPECT_EQ(mktime(&o1), o2.tv_sec) << "FAILED: " + c.buf;
    }
#endif
}

void TimeUtilUnittest::TestNativeStrptimeFormat() {
#ifdef __linux__
    // Only linux supports native strptime.
    // Only test timestamp converting now (UTF+8 is assumed).
    // TODO: Add more common cases in future.

    std::string format = "%s";
    std::string str = "1551053999";
    struct tm result = {0};
    auto ret = strptime(str.c_str(), format.c_str(), &result);
    ASSERT_TRUE(ret != NULL);
    EXPECT_EQ(2019, result.tm_year + 1900);
    EXPECT_EQ(2, result.tm_mon + 1);
    EXPECT_EQ(25, result.tm_mday);
    EXPECT_EQ(0, result.tm_hour - result.tm_gmtoff / 3600);
    EXPECT_EQ(19, result.tm_min);
    EXPECT_EQ(59, result.tm_sec);
#endif
}

void TimeUtilUnittest::TestStrptimeNanosecond() {
#ifdef __linux__
    // Only linux supports native strptime.
    struct Case {
        std::string buf1;
        std::string format1;
        std::string buf2;
        std::string format2;
        long expectedNanosecond;
    };

    std::vector<Case> cases{
        {"2012-01-01 15:05:07.123456", "%Y-%m-%d %H:%M:%S.%f", "2012-01-01 15:05:07", "%Y-%m-%d %H:%M:%S", 123456000},
        {"[2012-01-01 15:05:07.123456]",
         "[%Y-%m-%d %H:%M:%S.%f]",
         "[2012-01-01 15:05:07]",
         "[%Y-%m-%d %H:%M:%S]",
         123456000},
        {"01 Jan 12 15:05:07.123456 MST",
         "%d %b %y %H:%M:%S.%f",
         "01 Jan 12 15:05:07 MST",
         "%d %b %y %H:%M:%S",
         123456000},
        {"01 Jan 12 15:05:07.123456 -0700",
         "%d %b %y %H:%M:%S.%f",
         "01 Jan 12 15:05:07 -0700",
         "%d %b %y %H:%M:%S",
         123456000},
        {"Sunday, 01-Jan-12 15:05:07.123456 MST",
         "%A, %d-%b-%y %H:%M:%S.%f",
         "Sunday, 01-Jan-12 15:05:07 MST",
         "%A, %d-%b-%y %H:%M:%S",
         123456000},
        {"Sun, 01 Jan 12 15:05:07.123456 MST",
         "%A, %d %b %Y %H:%M:%S.%f",
         "Sun, 01 Jan 12 15:05:07 MST",
         "%A, %d %b %Y %H:%M:%S",
         123456000},
        {"2012-01-01T15:05:07.123456Z07:00",
         "%Y-%m-%dT%H:%M:%S.%f",
         "2012-01-01T15:05:07Z07:00",
         "%Y-%m-%dT%H:%M:%S",
         123456000},
        {"1325430307", "%s", "1325430307", "%s", 0},
        {"325430307", "%s", "325430307", "%s", 0},
        // the behavior of `strptime_ns` is different with C++ `strptime` in this case
        // {"1325430307123456", "%s", "1325430307123456", "%s", 123456000},
        {"15:05:07.123456 2012-01-01", "%H:%M:%S.%f %Y-%m-%d", "15:05:07 2012-01-01", "%H:%M:%S %Y-%m-%d", 123456000},
    };

    for (auto& c : cases) {
        struct tm o1 = {0};
        struct tm o2 = {0};
        long nanosecond = 0;
        int nanosecondLength;
        auto ret1 = strptime_ns(c.buf1.c_str(), c.format1.c_str(), &o1, &nanosecond, &nanosecondLength);
        auto ret2 = strptime(c.buf2.c_str(), c.format2.c_str(), &o2);

        EXPECT_TRUE(ret1 != NULL) << "FAILED: " + c.buf1;
        EXPECT_TRUE(ret2 != NULL) << "FAILED: " + c.buf2;
        EXPECT_EQ(mktime(&o1), mktime(&o2)) << "FAILED: " + c.buf1;
        EXPECT_EQ(nanosecond, c.expectedNanosecond) << "FAILED: " + c.buf1;
    }
#endif
}

void TimeUtilUnittest::TestGetPreciseTimestampFromLogtailTime() {
    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    preciseTimestampConfig.unit = TimeStampUnit::SECOND;
    LogtailTime lt;
    lt.tv_sec = 1640970061;
    lt.tv_nsec = 123456789;
    EXPECT_EQ(1640970061UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));

    preciseTimestampConfig.unit = TimeStampUnit::MILLISECOND;
    lt.tv_nsec = 0;
    EXPECT_EQ(1640970061000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 100000000;
    EXPECT_EQ(1640970061100UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 120000000;
    EXPECT_EQ(1640970061120UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123000000;
    EXPECT_EQ(1640970061123UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));

    preciseTimestampConfig.unit = TimeStampUnit::MICROSECOND;
    lt.tv_nsec = 0;
    EXPECT_EQ(1640970061000000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 100000000;
    EXPECT_EQ(1640970061100000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 120000000;
    EXPECT_EQ(1640970061120000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123000000;
    EXPECT_EQ(1640970061123000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123400000;
    EXPECT_EQ(1640970061123400UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123450000;
    EXPECT_EQ(1640970061123450UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123456000;
    EXPECT_EQ(1640970061123456UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));

    preciseTimestampConfig.unit = TimeStampUnit::NANOSECOND;
    lt.tv_nsec = 0;
    EXPECT_EQ(1640970061000000000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 100000000;
    EXPECT_EQ(1640970061100000000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 120000000;
    EXPECT_EQ(1640970061120000000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123000000;
    EXPECT_EQ(1640970061123000000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123400000;
    EXPECT_EQ(1640970061123400000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123450000;
    EXPECT_EQ(1640970061123450000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123456000;
    EXPECT_EQ(1640970061123456000UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123456700;
    EXPECT_EQ(1640970061123456700UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123456780;
    EXPECT_EQ(1640970061123456780UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
    lt.tv_nsec = 123456789;
    EXPECT_EQ(1640970061123456789UL, GetPreciseTimestampFromLogtailTime(lt, preciseTimestampConfig));
}

} // namespace logtail
