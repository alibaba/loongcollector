// // Copyright 2024 iLogtail Authors
// //
// // Licensed under the Apache License, Version 2.0 (the "License");
// // you may not use this file except in compliance with the License.
// // You may obtain a copy of the License at
// //
// //      http://www.apache.org/licenses/LICENSE-2.0
// //
// // Unless required by applicable law or agreed to in writing, software
// // distributed under the License is distributed on an "AS IS" BASIS,
// // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// // See the License for the specific language governing permissions and
// // limitations under the License.

// #include "ebpf/plugin/network_observer/Worker.h"
// #include "unittest/Unittest.h"

// namespace logtail {
// namespace ebpf {

// class WorkerUnittest : public ::testing::Test {
// public:
//     void TestWorkerPool();
//     void TestNetDataHandler();
//     void TestWorkerPoolShutdown();
//     void TestWorkerPoolMultipleThreads();
//     void TestWorkerPoolStress();

// protected:
//     void SetUp() override { ProtocolParserManager::GetInstance().AddParser(support_proto_e::ProtoHTTP); }
//     void TearDown() override {}
// };

// void WorkerUnittest::TestWorkerPool() {
//     // 创建输入输出队列
//     moodycamel::BlockingConcurrentQueue<std::unique_ptr<int>> inputQueue;
//     moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>> outputQueue;

//     // 创建处理函数
//     auto processFunc
//         = [](std::unique_ptr<int>& input, moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>>& output) {
//               if (input) {
//                   output.enqueue(std::make_shared<int>(*input * 2));
//               }
//           };

//     // 创建worker pool
//     WorkerPool<std::unique_ptr<int>, std::shared_ptr<int>> pool(inputQueue, outputQueue, processFunc, 2);

//     // 添加测试数据
//     inputQueue.enqueue(std::make_unique<int>(1));
//     inputQueue.enqueue(std::make_unique<int>(2));
//     inputQueue.enqueue(std::make_unique<int>(3));

//     // 验证结果
//     std::shared_ptr<int> result;
//     std::vector<int> results;
//     for (int i = 0; i < 3; ++i) {
//         APSARA_TEST_TRUE(outputQueue.wait_dequeue_timed(result, std::chrono::milliseconds(1000)));
//         if (result) {
//             results.push_back(*result);
//         }
//     }

//     // 验证结果是否正确
//     APSARA_TEST_EQUAL(results.size(), 3);
//     std::sort(results.begin(), results.end());
//     APSARA_TEST_EQUAL(results[0], 2);
//     APSARA_TEST_EQUAL(results[1], 4);
//     APSARA_TEST_EQUAL(results[2], 6);
// }

// void WorkerUnittest::TestNetDataHandler() {
//     // 创建输入输出队列
//     moodycamel::BlockingConcurrentQueue<std::unique_ptr<NetDataEvent>> inputQueue;
//     moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>> outputQueue;

//     // 创建 NetDataHandler
//     NetDataHandler handler;

//     const std::string resp = "HTTP/1.1 200 OK\r\n"
//                              "Content-Type: text/html\r\n"
//                              "Content-Length: 13\r\n"
//                              "\r\n"
//                              "Hello, World!";
//     const std::string req = "GET /index.html HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
//                             "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
//     std::string msg = req + resp;
//     // 创建测试数据
//     conn_data_event_t* evt = (conn_data_event_t*)malloc(offsetof(conn_data_event_t, msg) + msg.size());
//     memcpy(evt->msg, msg.data(), msg.size());
//     evt->conn_id.fd = 0;
//     evt->conn_id.start = 1;
//     evt->conn_id.tgid = 2;
//     evt->role = support_role_e::IsClient;
//     evt->request_len = req.size();
//     evt->response_len = resp.size();
//     evt->protocol = support_proto_e::ProtoHTTP;
//     evt->start_ts = 1;
//     evt->end_ts = 2;
//     auto event = std::make_unique<NetDataEvent>(evt);

//     // 处理数据
//     handler(event, outputQueue);

//     // 验证结果
//     std::shared_ptr<AbstractRecord> result;
//     APSARA_TEST_TRUE(outputQueue.try_dequeue(result));
//     APSARA_TEST_TRUE(result != nullptr);
//     free(evt);
// }

// void WorkerUnittest::TestWorkerPoolShutdown() {
//     moodycamel::BlockingConcurrentQueue<std::unique_ptr<int>> inputQueue;
//     moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>> outputQueue;

//     auto processFunc
//         = [](std::unique_ptr<int>& input, moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>>& output) {
//               if (input) {
//                   output.enqueue(std::make_shared<int>(*input));
//               }
//           };

//     {
//         // WorkerPool 在作用域结束时应该正常关闭
//         WorkerPool<std::unique_ptr<int>, std::shared_ptr<int>> pool(inputQueue, outputQueue, processFunc, 2);

//         // 添加一些数据
//         inputQueue.enqueue(std::make_unique<int>(1));
//         inputQueue.enqueue(std::make_unique<int>(2));
//         inputQueue.enqueue(std::make_unique<int>(3));

//         std::vector<std::shared_ptr<int>> items(3);
//         std::this_thread::sleep_for(std::chrono::milliseconds(500));
//         auto count = outputQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(500));
//         APSARA_TEST_EQUAL(count, 3);
//     }

//     // WorkerPool 销毁后,不应该再处理新数据
//     inputQueue.enqueue(std::make_unique<int>(3));
//     std::shared_ptr<int> result;
//     APSARA_TEST_FALSE(outputQueue.wait_dequeue_timed(result, std::chrono::milliseconds(500)));
// }

// void WorkerUnittest::TestWorkerPoolMultipleThreads() {
//     moodycamel::BlockingConcurrentQueue<std::unique_ptr<int>> inputQueue;
//     moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>> outputQueue;

//     auto processFunc
//         = [](std::unique_ptr<int>& input, moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>>& output) {
//               if (input) {
//                   std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟处理时间
//                   output.enqueue(std::make_shared<int>(*input));
//               }
//           };

//     const int threadCount = 4;
//     const int itemCount = 100;

//     WorkerPool<std::unique_ptr<int>, std::shared_ptr<int>> pool(inputQueue, outputQueue, processFunc, threadCount);

//     // 添加测试数据
//     for (int i = 0; i < itemCount; ++i) {
//         inputQueue.enqueue(std::make_unique<int>(i));
//     }

//     // 收集结果
//     std::vector<int> results;
//     std::shared_ptr<int> result;
//     for (int i = 0; i < itemCount; ++i) {
//         APSARA_TEST_TRUE(outputQueue.wait_dequeue_timed(result, std::chrono::milliseconds(1000)));
//         if (result) {
//             results.push_back(*result);
//         }
//     }

//     // 验证结果
//     APSARA_TEST_EQUAL(results.size(), itemCount);
//     std::sort(results.begin(), results.end());
//     for (int i = 0; i < itemCount; ++i) {
//         APSARA_TEST_EQUAL(results[i], i);
//     }
// }

// void WorkerUnittest::TestWorkerPoolStress() {
//     moodycamel::BlockingConcurrentQueue<std::unique_ptr<int>> inputQueue;
//     moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>> outputQueue;

//     auto processFunc
//         = [](std::unique_ptr<int>& input, moodycamel::BlockingConcurrentQueue<std::shared_ptr<int>>& output) {
//               if (input) {
//                   output.enqueue(std::make_shared<int>(*input));
//               }
//           };

//     const int threadCount = 8;
//     const int itemCount = 10000;

//     WorkerPool<std::unique_ptr<int>, std::shared_ptr<int>> pool(inputQueue, outputQueue, processFunc, threadCount);

//     // 创建生产者线程
//     std::thread producer([&]() {
//         for (int i = 0; i < itemCount; ++i) {
//             inputQueue.enqueue(std::make_unique<int>(i));
//             if (i % 100 == 0) {
//                 std::this_thread::sleep_for(std::chrono::milliseconds(1));
//             }
//         }
//     });

//     // 创建消费者线程
//     std::atomic<int> processedCount{0};
//     std::thread consumer([&]() {
//         std::shared_ptr<int> result;
//         while (processedCount < itemCount) {
//             if (outputQueue.wait_dequeue_timed(result, std::chrono::milliseconds(1000))) {
//                 if (result) {
//                     processedCount++;
//                 }
//             }
//         }
//     });

//     producer.join();
//     consumer.join();

//     APSARA_TEST_EQUAL(processedCount.load(), itemCount);
// }

// UNIT_TEST_CASE(WorkerUnittest, TestWorkerPool);
// UNIT_TEST_CASE(WorkerUnittest, TestNetDataHandler);
// UNIT_TEST_CASE(WorkerUnittest, TestWorkerPoolShutdown);
// UNIT_TEST_CASE(WorkerUnittest, TestWorkerPoolMultipleThreads);
// UNIT_TEST_CASE(WorkerUnittest, TestWorkerPoolStress);

// } // namespace ebpf
// } // namespace logtail

// UNIT_TEST_MAIN
