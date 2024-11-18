/*
 * Copyright 2024 iLogtail Authors
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

#pragma once

#include <boost/filesystem.hpp>

namespace bfs = boost::filesystem;

namespace logtail {

#ifndef APSARA_UNIT_TEST_MAIN
extern const bfs::path PROCESS_DIR;
#else
extern bfs::path PROCESS_DIR;
#endif

const extern bfs::path PROCESS_STAT;

} // namespace logtail
