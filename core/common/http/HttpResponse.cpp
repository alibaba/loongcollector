// Copyright 2024 iLogtail Authors
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

#include "common/http/HttpResponse.h"

using namespace std;

namespace logtail {

bool caseInsensitiveComp(const char lhs, const char rhs) {
    return tolower(lhs) < tolower(rhs);
}

bool compareHeader(const std::string& lhs, const std::string& rhs) {
    return lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), caseInsensitiveComp);
}

size_t DefaultWriteCallback(char* buffer, size_t size, size_t nmemb, void* data) {
    unsigned long sizes = size * nmemb;

    if (buffer == NULL) {
        return 0;
    }

    static_cast<string*>(data)->append(buffer, sizes);
    return sizes;
}

} // namespace logtail
