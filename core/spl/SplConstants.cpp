/*
 * Copyright 2023 iLogtail Authors
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

#include "spl/SplConstants.h"

namespace apsara::sls::spl {
const std::string FIELD_TIMESTAMP = "__timestamp__";
const std::string FIELD_TIMESTAMP_NANOSECOND = "__timestampNanosecond__";
const std::string FIELD_PREFIX_TAG = "__tag__:";
const std::string FIELD_CONTENT = "content";


const size_t LENGTH_FIELD_PREFIX_TAG = FIELD_PREFIX_TAG.length();
const size_t LENGTH_FIELD_TIMESTAMP = FIELD_TIMESTAMP.length();
const size_t LENGTH_FIELD_TIMESTAMP_NANOSECOND = FIELD_TIMESTAMP_NANOSECOND.length();

const std::string NULL_STR = "null";

} // namespace apsara::sls::spl
