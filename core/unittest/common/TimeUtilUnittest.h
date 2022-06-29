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

#include "unittest/Unittest.h"
#include <string>
#include <vector>
#include "common/TimeUtil.h"
#include "common/StringTools.h"

namespace logtail {

int DeduceYear(const struct tm* tm, const struct tm* currentTm);

class TimeUtilUnittest : public ::testing::Test {
public:
    void TestDeduceYear();
    void TestStrptime();
    void TestNativeStrptimeFormat();
    void TestGetPreciseTimestamp();
};

APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestDeduceYear, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestStrptime, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestNativeStrptimeFormat, 0);
APSARA_UNIT_TEST_CASE(TimeUtilUnittest, TestGetPreciseTimestamp, 0);

void TimeUtilUnittest::TestDeduceYear() {
    struct Case {
        std::string input;
        std::string current;
        int expected;
    };
    auto Parse = [](const std::string& s, struct tm* t) {
        auto sp = SplitString(s, "/");
        t->tm_year = StringTo<int>(sp[0]) - 1900;
        t->tm_mon = StringTo<int>(sp[1]) - 1;
        t->tm_mday = StringTo<int>(sp[2]);
    };

    std::vector<Case> cases{{"0/12/31", "2018/1/1", 2017},
                            {"0/1/1", "2018/12/31", 2019},
                            {"0/2/28", "2018/3/1", 2018},
                            {"0/2/29", "2016/2/29", 2016},
                            // We can not deduce for history logs.
                            {"0/9/9", "2018/1/1", 2018}};

    for (auto& c : cases) {
        struct tm input, current;
        Parse(c.input, &input);
        Parse(c.current, &current);
        EXPECT_EQ(c.expected, 1900 + DeduceYear(&input, &current));
    }
}

void TimeUtilUnittest::TestStrptime() {
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
        struct tm o2 = {0};
        auto ret1 = strptime(c.buf.c_str(), c.format.c_str(), &o1);
        auto ret2 = Strptime(c.buf.c_str(), c.format.c_str(), &o2, c.specifiedYear);

        EXPECT_TRUE(ret1 != NULL);
        EXPECT_TRUE(ret2 != NULL);
        EXPECT_EQ(o1.tm_mon, o2.tm_mon);
        EXPECT_EQ(o1.tm_mday, o2.tm_mday);

        EXPECT_EQ(c.expectedYear, 1900 + o2.tm_year);
    }
}

void TimeUtilUnittest::TestNativeStrptimeFormat() {
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
    EXPECT_EQ(8, result.tm_hour);
    EXPECT_EQ(19, result.tm_min);
    EXPECT_EQ(59, result.tm_sec);
}

void TimeUtilUnittest::TestGetPreciseTimestamp() {
    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    preciseTimestampConfig.unit = TimeStampUnit::SECOND;
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, NULL, preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, "", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, ".", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, " MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, " -0700", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, ".123", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, ".123", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, ",123Z07:00", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, " 123", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, ":123", preciseTimestampConfig));
    EXPECT_EQ(1640970061, GetPreciseTimestamp(1640970061, ":123 MST", preciseTimestampConfig));

    preciseTimestampConfig.unit = TimeStampUnit::MILLISECOND;
    EXPECT_EQ(1640970061000, GetPreciseTimestamp(1640970061, "", preciseTimestampConfig));
    EXPECT_EQ(1640970061000, GetPreciseTimestamp(1640970061, ".", preciseTimestampConfig));
    EXPECT_EQ(1640970061100, GetPreciseTimestamp(1640970061, ".1", preciseTimestampConfig));
    EXPECT_EQ(1640970061120, GetPreciseTimestamp(1640970061, ",12 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061123, GetPreciseTimestamp(1640970061, ".123", preciseTimestampConfig));
    EXPECT_EQ(1640970061123, GetPreciseTimestamp(1640970061, ",123Z07:00", preciseTimestampConfig));
    EXPECT_EQ(1640970061123, GetPreciseTimestamp(1640970061, " 123", preciseTimestampConfig));
    EXPECT_EQ(1640970061123, GetPreciseTimestamp(1640970061, ":123", preciseTimestampConfig));
    EXPECT_EQ(1640970061123, GetPreciseTimestamp(1640970061, ":123 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061123, GetPreciseTimestamp(1640970061, ".123456 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061000, GetPreciseTimestamp(1640970061, " -0700", preciseTimestampConfig));

    preciseTimestampConfig.unit = TimeStampUnit::MICROSECOND;
    EXPECT_EQ(1640970061000000, GetPreciseTimestamp(1640970061, "", preciseTimestampConfig));
    EXPECT_EQ(1640970061000000, GetPreciseTimestamp(1640970061, ".", preciseTimestampConfig));
    EXPECT_EQ(1640970061100000, GetPreciseTimestamp(1640970061, ".1", preciseTimestampConfig));
    EXPECT_EQ(1640970061120000, GetPreciseTimestamp(1640970061, ",12 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061123000, GetPreciseTimestamp(1640970061, ".123 4", preciseTimestampConfig));
    EXPECT_EQ(1640970061123000, GetPreciseTimestamp(1640970061, ",123Z07:00", preciseTimestampConfig));
    EXPECT_EQ(1640970061123400, GetPreciseTimestamp(1640970061, ",1234Z07:00", preciseTimestampConfig));
    EXPECT_EQ(1640970061123450, GetPreciseTimestamp(1640970061, " 12345", preciseTimestampConfig));
    EXPECT_EQ(1640970061123456, GetPreciseTimestamp(1640970061, ":1234567", preciseTimestampConfig));
    EXPECT_EQ(1640970061123400, GetPreciseTimestamp(1640970061, ":1234 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061123456, GetPreciseTimestamp(1640970061, ".123456 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061000000, GetPreciseTimestamp(1640970061, " -0700", preciseTimestampConfig));

    preciseTimestampConfig.unit = TimeStampUnit::NANOSECOND;
    EXPECT_EQ(1640970061000000000, GetPreciseTimestamp(1640970061, "", preciseTimestampConfig));
    EXPECT_EQ(1640970061000000000, GetPreciseTimestamp(1640970061, ".", preciseTimestampConfig));
    EXPECT_EQ(1640970061100000000, GetPreciseTimestamp(1640970061, ".1", preciseTimestampConfig));
    EXPECT_EQ(1640970061120000000, GetPreciseTimestamp(1640970061, ",12 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061123000000, GetPreciseTimestamp(1640970061, ".123 4", preciseTimestampConfig));
    EXPECT_EQ(1640970061123000000, GetPreciseTimestamp(1640970061, ",123Z07:00", preciseTimestampConfig));
    EXPECT_EQ(1640970061123400000, GetPreciseTimestamp(1640970061, ",1234Z07:00", preciseTimestampConfig));
    EXPECT_EQ(1640970061123450000, GetPreciseTimestamp(1640970061, " 12345", preciseTimestampConfig));
    EXPECT_EQ(1640970061123456700, GetPreciseTimestamp(1640970061, ":1234567", preciseTimestampConfig));
    EXPECT_EQ(1640970061123400000, GetPreciseTimestamp(1640970061, ":1234 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061123456000, GetPreciseTimestamp(1640970061, ".123456 MST", preciseTimestampConfig));
    EXPECT_EQ(1640970061000000000, GetPreciseTimestamp(1640970061, " -0700", preciseTimestampConfig));
}

} // namespace logtail
