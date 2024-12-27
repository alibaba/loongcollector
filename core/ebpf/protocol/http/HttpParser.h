// //
// // Created by qianlu on 2024/5/20.
// //

// #pragma once

// #include <vector>
// #include <memory>
// #include <iostream>
// #include <map>

// #include "picohttpparser.h"

// namespace logtail {
// constexpr size_t kMaxNumHeaders = 50;

// struct State {
//   bool conn_closed = false;
// };


// struct HTTPRequest {
//   const char* method = nullptr;
//   size_t method_len = 0;
//   const char* path = nullptr;
//   size_t path_len = 0;
//   int minor_version = 0;
//   struct phr_header headers[kMaxNumHeaders];
//   // Set header number to maximum we can accept.
//   // Pico will change it to the number of headers parsed for us.
//   size_t num_headers = kMaxNumHeaders;
// };

// struct HTTPResponse {
//   const char* msg = nullptr;
//   size_t msg_len = 0;
//   int status = 0;
//   int minor_version = 0;
//   struct phr_header headers[kMaxNumHeaders];
//   // Set header number to maximum we can accept.
//   // Pico will change it to the number of headers parsed for us.
//   size_t num_headers = kMaxNumHeaders;
// };

// struct Message {
//   int minor_version = -1;
//     HeadersMap headers = {};

//   std::string req_method = "-";
//   std::string req_path = "-";

//   int resp_status = -1;
//   std::string resp_message = "-";

//   std::string body = "-";
//   size_t body_size = 0;

//   size_t headers_byte_size = 0;
// };

// enum class ParseState {
//   kUnknown,

//   // The parse failed: data is invalid.
//   // Input buffer consumed is not consumed and parsed output element is invalid.
//   kInvalid,

//   // The parse is partial: data appears to be an incomplete message.
//   // Input buffer may be partially consumed and the parsed output element is not fully populated.
//   kNeedsMoreData,

//   // The parse succeeded, but the data is ignored.
//   // Input buffer is consumed, but the parsed output element is invalid.
//   kIgnored,

//   // The parse succeeded, but indicated the end-of-stream.
//   // Input buffer is consumed, and the parsed output element is valid.
//   // however, caller should stop parsing any future data on this stream, even if more data exists.
//   // Use cases include messages that indicate a change in protocol (see HTTP status 101).
//   kEOS,

//   // The parse succeeded.
//   // Input buffer is consumed, and the parsed output element is valid.
//   kSuccess,
// };

// namespace http {

// ParseState ParseRequest(std::string_view* buf, Message* result);

// ParseState ParseRequestBody(std::string_view* buf, Message* result);

// HeadersMap GetHTTPHeadersMap(const phr_header* headers, size_t num_headers);

// ParseState ParseContent(std::string_view content_len_str, std::string_view* data,
//                             size_t body_size_limit_bytes, std::string* result, size_t* body_size);

// ParseState ParseResponse(std::string_view* buf, Message* result, bool closed);

// int ParseHttpRequest(std::string_view buf, HTTPRequest* result);
// }



// class HTTPProtocolParser : public AbstractProtocolParser {
//   public:
//   std::shared_ptr<AbstractProtocolParser> Create() override {
//     return std::make_shared<HTTPProtocolParser>();
//   }

//   std::vector<std::unique_ptr<AbstractRecord>> Parse(std::unique_ptr<NetDataEvent> data_event) override;
// };

// REGISTER_PROTOCOL_PARSER(ProtocolType::HTTP, HTTPProtocolParser)

// }
