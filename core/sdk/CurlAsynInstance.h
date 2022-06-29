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
#include "Common.h"
#include <queue>
#include <boost/thread.hpp>
#include <curl/curl.h>

namespace logtail {
namespace sdk {

#define LOGTAIL_SDK_CURL_THREAD_POOL_SIZE (1)

    class CurlAsynInstance {
    public:
        static CurlAsynInstance* GetInstance() {
            static CurlAsynInstance* sInstance = new CurlAsynInstance();
            return sInstance;
        }

        CurlAsynInstance();

        ~CurlAsynInstance();

        template <typename Data>
        class RequestQueue {
        private:
            std::queue<Data> the_queue;
            mutable boost::mutex the_mutex;
            boost::condition_variable the_condition_variable;

        public:
            void push(Data const& data) {
                boost::mutex::scoped_lock lock(the_mutex);
                the_queue.push(data);
                lock.unlock();
                the_condition_variable.notify_one();
            }

            bool empty() const {
                boost::mutex::scoped_lock lock(the_mutex);
                return the_queue.empty();
            }

            bool try_pop(Data& popped_value) {
                boost::mutex::scoped_lock lock(the_mutex);
                if (the_queue.empty()) {
                    return false;
                }

                popped_value = the_queue.front();
                the_queue.pop();
                return true;
            }

            // wait_and_pop will block at most 500ms to avoid missing the last message (rare case).
            bool wait_and_pop(Data& popped_value) {
                boost::mutex::scoped_lock lock(the_mutex);
                while (the_queue.empty()) {
                    if (boost::cv_status::timeout
                        == the_condition_variable.wait_for(lock, boost::chrono::milliseconds(500)))
                        return false;
                }

                popped_value = the_queue.front();
                the_queue.pop();
                return true;
            }
        };

        void AddRequest(AsynRequest* request) { mRequestQueue.push(request); }

        void Run();

        bool MultiHandlerLoop(CURLM* multiHandler);

    private:
        RequestQueue<AsynRequest*> mRequestQueue;
        std::vector<boost::thread*> mMainThreads;
    };

} // namespace sdk
} // namespace logtail
