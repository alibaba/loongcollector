// Copyright 2023 iLogtail Authors
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
#include "common/EncodingConverter.h"
#if defined(__linux__)
#include "unittest/UnittestHelper.h"
#endif

namespace logtail {

class EncodingConverterUnittest : public ::testing::Test {
public:
    void ConvertGbk2Utf8();
};

APSARA_UNIT_TEST_CASE(EncodingConverterUnittest, ConvertGbk2Utf8, 0);

void EncodingConverterUnittest::ConvertGbk2Utf8() {
    char gbkStr[] = "ilogtail\xbf\xc9\xb9\xdb\xb2\xe2\xd0\xd4\xb2\xc9\xbc\xaf\xc6\xf7";
    size_t srcLen = strlen(gbkStr);
    std::vector<long> linePosVec = {long(srcLen)};
    size_t requireSize = EncodingConverter::GetInstance()->ConvertGbk2Utf8(gbkStr, &srcLen, nullptr, 0, linePosVec) + 1;
    APSARA_TEST_GT(requireSize, 1UL);
    std::unique_ptr<char[]> destChar(new char[requireSize]);
    size_t actualSize
        = EncodingConverter::GetInstance()->ConvertGbk2Utf8(gbkStr, &srcLen, destChar.get(), requireSize, linePosVec);
    APSARA_TEST_GT(actualSize, 0UL);
    destChar[actualSize + 1] = '\0';
    APSARA_TEST_STREQ("ilogtail可观测性采集器", destChar.get());
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
