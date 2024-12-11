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

#include "common/http/HttpRequest.h"

DEFINE_FLAG_INT32(default_http_request_timeout_secs, "", 15);
DEFINE_FLAG_INT32(default_http_request_max_try_cnt, "", 3);

using namespace std;

namespace logtail {

static unsigned char ToHex(unsigned char x) {
    return x > 9 ? x + 55 : x + 48;
}

static string UrlEncode(const string& str) {
    string strTemp;
    size_t length = str.length();
    for (size_t i = 0; i < length; i++) {
        if (isalnum((unsigned char)str[i]) || (str[i] == '-') || (str[i] == '_') || (str[i] == '.') || (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ')
            strTemp += "+";
        else {
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] % 16);
        }
    }
    return strTemp;
}

string GetQueryString(const map<string, string>& parameters) {
    string res;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        if (it != parameters.begin()) {
            res.append("&");
        }
        res.append(it->first);
        res.append("=");
        res.append(UrlEncode(it->second));
    }
    return res;
}

} // namespace logtail
