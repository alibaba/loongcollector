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

#include "network/protocols/category.h"
#include "network/protocols/common.h"

namespace logtail {
using DNSProtocolEventKey = RequestAggKey<ProtocolType_DNS>;
using DNSProtocolEvent = CommonProtocolEvent<DNSProtocolEventKey>;
using DNSProtocolEventAggItem = CommonProtocolEventAggItem<DNSProtocolEventKey, CommonProtocolAggResult>;
using DNSProtocolEventAggItemManager = CommonProtocolEventAggItemManager<DNSProtocolEventAggItem>;
using DNSProtocolEventAggregator = CommonProtocolEventAggregator<DNSProtocolEvent,
                                                                 DNSProtocolEventAggItem,
                                                                 DNSProtocolEventAggItemManager>;

} // namespace logtail
