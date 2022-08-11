// Copyright 2022 iLogtail Authors
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

#include "ProtocolEventAggregators.h"

namespace logtail {


void ProtocolEventAggregators::FlushOutMetrics(uint64_t timeNano,
                                               std::vector<sls_logs::Log>& allData,
                                               std::vector<std::pair<std::string, std::string> >& processTags,
                                               std::vector<std::pair<std::string, std::string> >& globalTags) {
    ::google::protobuf::RepeatedPtrField<sls_logs::Log_Content> tagsContent;
    tagsContent.Reserve(processTags.size() + globalTags.size() + 1);
    {
        sls_logs::Log_Content* content = tagsContent.Add();
        content->set_key("type");
        content->set_value("protocols");
    }
    for (auto& tag : processTags) {
        sls_logs::Log_Content* content = tagsContent.Add();
        content->set_key(tag.first);
        content->set_value(tag.second);
    }
    for (const auto& tag : globalTags) {
        sls_logs::Log_Content* content = tagsContent.Add();
        content->set_key(tag.first);
        content->set_value(tag.second);
    }
    if (mDNSAggregators != NULL) {
        mDNSAggregators->FlushLogs(allData, tagsContent);
    }

    if (mHTTPAggregators != NULL) {
        mHTTPAggregators->FlushLogs(allData, tagsContent);
    }

    if (mMySQLAggregators != NULL) {
        mMySQLAggregators->FlushLogs(allData, tagsContent);
    }

    if (mRedisAggregators != NULL) {
        mRedisAggregators->FlushLogs(allData, tagsContent);
    }

    if (mPgSQLAggregators != NULL) {
        mPgSQLAggregators->FlushLogs(allData, tagsContent);
    }
}

} // namespace logtail
