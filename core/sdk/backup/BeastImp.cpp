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

#include "BeastImp.h"

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <memory>

using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http; // from <boost/beast/http.hpp>

namespace logtail {
namespace sdk {

    static http::verb GetHttpMethod(const std::string& httpMethod) {
        if (httpMethod == HTTP_POST) {
            return http::verb::post;
        } else if (httpMethod == HTTP_GET) {
            return http::verb::get;
        } else if (httpMethod == HTTP_PUT) {
            return http::verb::put;
        } else if (httpMethod == HTTP_DELETE) {
            return http::verb::delete_;
        }
        return http::verb::post;
    }

    void BeastClient::Send(const std::string& httpMethod,
                           const std::string& host,
                           const int32_t port,
                           const std::string& url,
                           const std::string& queryString,
                           const std::map<std::string, std::string>& header,
                           const std::string& body,
                           const int32_t timeout,
                           HttpMessage& httpMessage,
                           const std::string& interface,
                           const bool httpsFlag) {
        // Declare a container to hold the response
        http::response<http::dynamic_body> res;
        try {
            // The io_context is required for all I/O
            boost::asio::io_context ioc;

            // These objects perform our I/O
            tcp::resolver resolver{ioc};
            tcp::socket socket{ioc};

            // Look up the domain name
            auto const results = resolver.resolve(host, std::to_string(port));

            std::cout << "host : " << host << "port : " << port << std::endl;

            // Make the connection on the IP address we get from a lookup
            boost::asio::connect(socket, results.begin(), results.end());

            std::cout << "connect" << std::endl;

            // Set up an HTTP GET request message
            int version = 11;
            http::request<http::string_body> req{GetHttpMethod(httpMethod), url, version};
            for (auto iter = header.begin(); iter != header.end(); iter++) {
                req.set(iter->first, iter->second);
            }
            req.body() = body;

            // Send the HTTP request to the remote host
            std::cout << "write" << std::endl;
            http::write(socket, req);
            std::cout << "write done" << std::endl;

            // This buffer is used for reading and must be persisted
            boost::beast::flat_buffer buffer;

            // Receive the HTTP response
            std::cout << "read" << std::endl;
            http::read(socket, buffer, res);
            std::cout << "read done" << std::endl;

            // Write the message to standard out
            std::cout << "response : " << res << std::endl;

            // Gracefully close the socket
            boost::system::error_code ec;
            socket.shutdown(tcp::socket::shutdown_both, ec);

            // not_connected happens sometimes
            // so don't bother reporting it.
            //
            if (ec && ec != boost::system::errc::not_connected)
                throw boost::system::system_error{ec};

            // If we get here then the connection is closed gracefully
        } catch (std::exception const& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            for (auto iter = res.begin(); iter != res.end(); ++iter) {
                auto iter_cp = iter;
                //std::cerr << "header : " << std::to_string(*iter) << std::endl;
            }
            //std::cerr << "Response : " << res.begin() << std::endl;
        }
    }
    void BeastClient::AsynSend(const std::string& httpMethod,
                               const std::string& host,
                               const int32_t port,
                               const std::string& url,
                               const std::string& queryString,
                               const std::map<std::string, std::string>& header,
                               const std::string& body,
                               const int32_t timeout,
                               LogsClosure* callBack,
                               const std::string& interface,
                               const bool httpsFlag) {}

} // namespace sdk

} // namespace logtail
