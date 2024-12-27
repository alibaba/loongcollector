// // Copyright 2023 iLogtail Authors
// //
// // Licensed under the Apache License, Version 2.0 (the "License");
// // you may not use this file except in compliance with the License.
// // You may obtain a copy of the License at
// //
// //     http://www.apache.org/licenses/LICENSE-2.0
// //
// // Unless required by applicable law or agreed to in writing, software
// // distributed under the License is distributed on an "AS IS" BASIS,
// // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// // See the License for the specific language governing permissions and
// // limitations under the License.

// #pragma once

// #include <vector>
// #include <memory>
// #include <unordered_map>
// #include <iostream>

// #include "type/abstract_record.h"
// #include "type/abstract_raw_event.h"
// #include "type/common.h"
// #include "type/ebpf_type.h"
// #include "abstract_parser.h"
// #include "parser_registry.h"

// /* include all the parsers to register them for side effect */
// #include "http/HttpParser.h"
// /* register end*/


// namespace logtail {
// // TODO @qianlu.kk maybe we should wrapped result into another struct
// //    to record the status
// class ParseResult {
//   int status;
//   std::vector<std::unique_ptr<AbstractRecord>> records;
// };

// class ProtocolParserManager {
// public:
//   ProtocolParserManager(const ProtocolParserManager&) = delete;
//   ProtocolParserManager& operator=(const ProtocolParserManager&) = delete;

//   // singleton
//   static ProtocolParserManager& GetInstance() {
//     static ProtocolParserManager instance;
//     return instance;
//   }

//   void Init() {
//     for (auto type : {ProtocolType::HTTP, ProtocolType::DNS, ProtocolType::KAFKA}) {
//       auto parser = ProtocolParserRegistry::instance().createParser(type);
//       if (parser) {
//         parsers_[type] = std::move(parser);
//       } else {
//         LOG(WARNING) << "No parser available for type " << static_cast<int>(type);
//       }
//     }
//   }

//   std::vector<std::unique_ptr<AbstractRecord>> Parse(ProtocolType type, std::unique_ptr<NetDataEvent> data) {
//     if (parsers_.find(type) != parsers_.end()) {
//       return parsers_[type]->Parse(std::move(data));
//       // do coverge
//       // do aggregate
//     } else {
//       LOG(WARNING) << "No parser found for given protocol type: " << int(type);
//       // TODO @qianlu.kk modify here
// //      return parsers_[ProtocolType::HTTP]->Parse(std::move(data));
//       return std::vector<std::unique_ptr<AbstractRecord>>();
//     }
//   }

// private:
//   ProtocolParserManager() {}
//   std::unordered_map<ProtocolType, std::shared_ptr<AbstractProtocolParser>> parsers_;
// };
// }

