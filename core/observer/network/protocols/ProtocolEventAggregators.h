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

#include "network/protocols/dns/type.h"
#include "network/protocols/http/type.h"
#include "network/protocols/mysql/type.h"
#include "network/protocols/redis/type.h"
#include "network/protocols/pgsql/type.h"
#include <unordered_map>
#include <metas/ProcessMeta.h>
#include <protobuf/sls/sls_logs.pb.h>


namespace logtail {

class ProtocolEventAggregators {
public:
    ProtocolEventAggregators() = default;

    ~ProtocolEventAggregators() {
        if (mDNSAggregators != NULL) {
            delete mDNSAggregators;
            mDNSAggregators = NULL;
        }
        if (mHTTPAggregators != NULL) {
            delete mHTTPAggregators;
            mHTTPAggregators = NULL;
        }
        if (mMySQLAggregators != NULL) {
            delete mMySQLAggregators;
            mMySQLAggregators = NULL;
        }
        if (mRedisAggregators != NULL) {
            delete mRedisAggregators;
            mRedisAggregators = NULL;
        }
        if (mPgSQLAggregators != NULL) {
            delete mPgSQLAggregators;
            mPgSQLAggregators = NULL;
        }
    }

    DNSProtocolEventAggregator* GetDNSAggregator() {
        if (mDNSAggregators != NULL) {
            return mDNSAggregators;
        }
        auto pair = NetworkConfig::GetProtocolAggSize(ProtocolType_DNS);
        mDNSAggregators = new DNSProtocolEventAggregator(pair.first, pair.second);
        return mDNSAggregators;
    }

    HTTPProtocolEventAggregator* GetHTTPAggregator() {
        if (mHTTPAggregators != NULL) {
            return mHTTPAggregators;
        }
        auto pair = NetworkConfig::GetProtocolAggSize(ProtocolType_HTTP);
        mHTTPAggregators = new HTTPProtocolEventAggregator(pair.first, pair.second);
        return mHTTPAggregators;
    }


    MySQLProtocolEventAggregator* GetMySQLAggregator() {
        if (mMySQLAggregators != NULL) {
            return mMySQLAggregators;
        }
        auto pair = NetworkConfig::GetProtocolAggSize(ProtocolType_MySQL);
        mMySQLAggregators = new MySQLProtocolEventAggregator(pair.first, pair.second);
        return mMySQLAggregators;
    }

    RedisProtocolEventAggregator* GetRedisAggregator() {
        if (mRedisAggregators != NULL) {
            return mRedisAggregators;
        }
        auto pair = NetworkConfig::GetProtocolAggSize(ProtocolType_Redis);
        mRedisAggregators = new RedisProtocolEventAggregator(pair.first, pair.second);
        return mRedisAggregators;
    }

    PgSQLProtocolEventAggregator* GetPgSQLAggregator() {
        if (mPgSQLAggregators != NULL) {
            return mPgSQLAggregators;
        }
        auto pair = NetworkConfig::GetProtocolAggSize(ProtocolType_PgSQL);
        mPgSQLAggregators = new PgSQLProtocolEventAggregator(pair.first, pair.second);
        return mPgSQLAggregators;
    }

    const ProcessMetaPtr& GetProcessMeta() const { return mMetaPtr; }

    void SetProcessMeta(const ProcessMetaPtr& metaPtr) { mMetaPtr = metaPtr; }

    void FlushOutMetrics(uint64_t timeNano,
                         std::vector<sls_logs::Log>& allData,
                         std::vector<std::pair<std::string, std::string>>& processTags,
                         std::vector<std::pair<std::string, std::string>>& globalTags,
                         uint64_t interval);

protected:
    DNSProtocolEventAggregator* mDNSAggregators = NULL;
    HTTPProtocolEventAggregator* mHTTPAggregators = NULL;
    MySQLProtocolEventAggregator* mMySQLAggregators = NULL;
    RedisProtocolEventAggregator* mRedisAggregators = NULL;
    PgSQLProtocolEventAggregator* mPgSQLAggregators = NULL;
    ProcessMetaPtr mMetaPtr;
};

} // namespace logtail
