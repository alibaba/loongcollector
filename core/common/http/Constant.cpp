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

#include "common/http/Constant.h"

using namespace std;

namespace logtail {
    
const string HTTP_POST = "POST";
const string HTTP_GET = "GET";

const string HOST = "Host";
const string DATE = "Date";
const string USER_AGENT = "User-Agent";
const string CONTENT_TYPE = "Content-Type";
const string CONTENT_LENGTH = "Content-Length";
const string AUTHORIZATION = "Authorization";
const string SIGNATURE = "Signature";

const string TYPE_LOG_PROTOBUF = "application/x-protobuf";

} // namespace logtail
