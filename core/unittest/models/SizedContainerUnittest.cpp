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


UNIT_TEST_CASE(SizedContainerUnittest, TestInsertAndErase)

} // namespace logtail

UNIT_TEST_MAIN
