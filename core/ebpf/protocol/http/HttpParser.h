//
// Created by qianlu on 2024/5/20.
//

#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "ebpf/protocol/AbstractParser.h"
#include "ebpf/protocol/ParserRegistry.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/util/Converger.h"
#include "ebpf/util/sampler/Sampler.h"
#include "picohttpparser.h"

namespace logtail::ebpf {

constexpr size_t kMaxNumHeaders = 50;

struct HTTPRequest {
    const char* mMethod = nullptr;
    size_t mMethodLen = 0;
    const char* mPath = nullptr;
    size_t mPathLen = 0;
    int mMinorVersion = 0;
    struct phr_header mHeaders[kMaxNumHeaders];
    // Set header number to maximum we can accept.
    // Pico will change it to the number of headers parsed for us.
    size_t mNumHeaders = kMaxNumHeaders;
};

struct HTTPResponse {
    const char* mMsg = nullptr;
    size_t mMsgLen = 0;
    int mStatus = 0;
    int mMinorVersion = 0;
    struct phr_header mHeaders[kMaxNumHeaders];
    // Set header number to maximum we can accept.
    // Pico will change it to the number of headers parsed for us.
    size_t mNumHeaders = kMaxNumHeaders;
};

namespace http {

ParseState ParseRequest(std::string_view& buf, std::shared_ptr<HttpRecord>& result, bool forceSample = false);

ParseState ParseRequestBody(std::string_view& buf, std::shared_ptr<HttpRecord>& result);

HeadersMap GetHTTPHeadersMap(const phr_header* headers, size_t numHeaders);

ParseState ParseContent(std::string_view& contentLenStr,
                        std::string_view& data,
                        size_t bodySizeLimitBytes,
                        std::string& result,
                        size_t& bodySize);

ParseState
ParseResponse(std::string_view& buf, std::shared_ptr<HttpRecord>& result, bool closed, bool forceSample = false);

int ParseHttpRequest(std::string_view& buf, HTTPRequest& result);
} // namespace http


class HTTPProtocolParser : public AbstractProtocolParser {
public:
    std::shared_ptr<AbstractProtocolParser> Create() override { return std::make_shared<HTTPProtocolParser>(); }

    std::vector<std::shared_ptr<L7Record>> Parse(struct conn_data_event_t* dataEvent,
                                                 const std::shared_ptr<Connection>& conn,
                                                 const std::shared_ptr<AppDetail>& appDetail,
                                                 const std::shared_ptr<AppConvergerManager>& converger) override;
};

REGISTER_PROTOCOL_PARSER(support_proto_e::ProtoHTTP, HTTPProtocolParser)

} // namespace logtail::ebpf
