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

#pragma once

#include "interface/protocol.h"
#include "network/protocols/common.h"
#include "network/protocols/category.h"
#include <string>
#include <ostream>
#include "common/xxhash/xxhash.h"

namespace logtail {

using HTTPProtocolEventKey = RequestAggKey<ProtocolType_HTTP>;
using HTTPProtocolEvent = CommonProtocolEvent<HTTPProtocolEventKey>;
using HTTPProtocolEventAggItem = CommonProtocolEventAggItem<HTTPProtocolEventKey, CommonProtocolAggResult>;
using HTTPProtocolEventAggItemManager = CommonProtocolEventAggItemManager<HTTPProtocolEventAggItem>;
using HTTPProtocolEventAggregator = CommonProtocolEventAggregator<HTTPProtocolEvent,
                                                                  HTTPProtocolEventAggItem,
                                                                  HTTPProtocolEventAggItemManager>;

} // namespace logtail
