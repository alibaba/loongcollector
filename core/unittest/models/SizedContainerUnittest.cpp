// Copyright 2024 iLogtail Authors
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


#include "PipelineEvent.h"
#include "SizedContainer.h"
#include "common/StringView.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class SizedContainerUnittest : public ::testing::Test {
public:
    void TestInsertAndErase();
    void TestInsertWithReplace();

protected:
private:
};


void SizedContainerUnittest::TestInsertAndErase() {
    auto findTag = [](SizedVectorTags& tags, StringView key) {
        auto iter = std::find_if(
            tags.mInner.begin(), tags.mInner.end(), [key](const auto& item) { return item.first == key; });
        if (iter == tags.mInner.end()) {
            return gEmptyStringView;
        } else {
            return iter->second;
        }
    };
    SizedVectorTags tags;
    auto basicSize = sizeof(vector<std::pair<StringView, StringView>>);
    // insert one
    {
        tags.Clear();
        string key = "key1";
        string value = "value1";
        tags.Insert(key, value);
        APSARA_TEST_EQUAL(value, findTag(tags, key).to_string());
        APSARA_TEST_EQUAL(basicSize + 10, tags.DataSize());
    }
    // insert two
    {
        tags.Clear();
        string key = "key1";
        string value = "value1";
        tags.Insert(key, value);
        APSARA_TEST_EQUAL(basicSize + 10, tags.DataSize());
        string key2 = "key2";
        string value2 = "value2";
        tags.Insert(key2, value2);
        APSARA_TEST_EQUAL(basicSize + 20, tags.DataSize());
        APSARA_TEST_EQUAL(value, findTag(tags, key).to_string());
        APSARA_TEST_EQUAL(value2, findTag(tags, key2).to_string());
    }
    // insert by the same key
    {
        tags.Clear();
        string key = "key1";
        string value = "value1";
        tags.Insert(key, value);
        APSARA_TEST_EQUAL(basicSize + 10, tags.DataSize());
        string value22 = "value22";
        tags.Insert(key, value22);
        APSARA_TEST_EQUAL(basicSize + 11, tags.DataSize());
        APSARA_TEST_EQUAL(value22, findTag(tags, key).to_string());
    }

    // erase one
    {
        tags.Clear();
        string key = "key1";
        string value = "value1";
        tags.Insert(key, value);
        APSARA_TEST_EQUAL(value, findTag(tags, key).to_string());
        tags.Erase(key);
        APSARA_TEST_EQUAL("", findTag(tags, key).to_string());
        APSARA_TEST_EQUAL(basicSize, tags.DataSize());
    }
    // erase two
    {
        tags.Clear();
        string key = "key1";
        string value = "value1";
        tags.Insert(key, value);
        string key2 = "key2";
        string value2 = "value2";
        tags.Insert(key2, value2);
        APSARA_TEST_EQUAL(value, findTag(tags, key).to_string());
        tags.Erase(key);
        APSARA_TEST_EQUAL(basicSize + 10, tags.DataSize());
        APSARA_TEST_EQUAL("", findTag(tags, key).to_string());
        APSARA_TEST_EQUAL(value2, findTag(tags, key2).to_string());
        tags.Erase(key2);
        APSARA_TEST_EQUAL(basicSize, tags.DataSize());
        APSARA_TEST_EQUAL("", findTag(tags, key2).to_string());
    }
    // erase twice
    {
        tags.Clear();
        string key = "key1";
        string value = "value1";
        tags.Insert(key, value);
        APSARA_TEST_EQUAL(value, findTag(tags, key).to_string());
        tags.Erase(key);
        APSARA_TEST_EQUAL(basicSize, tags.DataSize());
        APSARA_TEST_EQUAL("", findTag(tags, key).to_string());
        tags.Erase(key);
        APSARA_TEST_EQUAL(basicSize, tags.DataSize());
        APSARA_TEST_EQUAL("", findTag(tags, key).to_string());
    }
}

void SizedContainerUnittest::TestInsertWithReplace() {
    auto findTag = [](SizedVectorTags& tags, StringView key) {
        auto iter = std::find_if(
            tags.mInner.begin(), tags.mInner.end(), [key](const auto& item) { return item.first == key; });
        if (iter == tags.mInner.end()) {
            return gEmptyStringView;
        } else {
            return iter->second;
        }
    };
    SizedVectorTags tags;
    auto basicSize = sizeof(vector<std::pair<StringView, StringView>>);

    // test replace=true (default behavior)
    {
        tags.Clear();
        string key = "key1";
        string value1 = "value1";
        tags.Insert(key, value1, true);
        APSARA_TEST_EQUAL_FATAL(value1, findTag(tags, key).to_string());
        APSARA_TEST_EQUAL_FATAL(1U, tags.mInner.size());
        APSARA_TEST_EQUAL_FATAL(basicSize + 10, tags.DataSize());

        // insert with same key and replace=true should replace the value
        string value2 = "value2";
        tags.Insert(key, value2, true);
        APSARA_TEST_EQUAL_FATAL(value2, findTag(tags, key).to_string());
        APSARA_TEST_EQUAL_FATAL(1U, tags.mInner.size());
        APSARA_TEST_EQUAL_FATAL(basicSize + 10, tags.DataSize());
    }

    // test replace=false (append behavior)
    {
        tags.Clear();
        string key = "key1";
        string value1 = "value1";
        tags.Insert(key, value1, false);
        APSARA_TEST_EQUAL_FATAL(value1, findTag(tags, key).to_string());
        APSARA_TEST_EQUAL_FATAL(1U, tags.mInner.size());
        APSARA_TEST_EQUAL_FATAL(basicSize + 10, tags.DataSize());

        // insert with same key and replace=false should append a new entry
        string value2 = "value2";
        tags.Insert(key, value2, false);
        // findTag returns the first match
        APSARA_TEST_EQUAL_FATAL(value1, findTag(tags, key).to_string());
        APSARA_TEST_EQUAL_FATAL(2U, tags.mInner.size());
        APSARA_TEST_EQUAL_FATAL(basicSize + 20, tags.DataSize());

        // verify both entries exist
        int count = 0;
        for (const auto& item : tags.mInner) {
            if (item.first == key) {
                count++;
            }
        }
        APSARA_TEST_EQUAL_FATAL(2, count);
    }

    // test mixed replace and no-replace operations
    {
        tags.Clear();
        string key1 = "key1";
        string key2 = "key2";
        string value1 = "value1";
        string value2 = "value2";
        string value3 = "value3";

        // insert key1 with replace=false
        tags.Insert(key1, value1, false);
        APSARA_TEST_EQUAL_FATAL(1U, tags.mInner.size());

        // insert key1 again with replace=false
        tags.Insert(key1, value2, false);
        APSARA_TEST_EQUAL_FATAL(2U, tags.mInner.size());

        // insert key1 with replace=true, should replace the first occurrence
        tags.Insert(key1, value3, true);
        APSARA_TEST_EQUAL_FATAL(2U, tags.mInner.size());
        APSARA_TEST_EQUAL_FATAL(value3, findTag(tags, key1).to_string());

        // insert key2 with replace=true
        tags.Insert(key2, value1, true);
        APSARA_TEST_EQUAL_FATAL(3U, tags.mInner.size());
    }
}


UNIT_TEST_CASE(SizedContainerUnittest, TestInsertAndErase)
UNIT_TEST_CASE(SizedContainerUnittest, TestInsertWithReplace)

} // namespace logtail

UNIT_TEST_MAIN
