// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/YamlUtil.h"

#include <string>

#include "common/ExceptionBase.h"

using namespace std;

namespace logtail {

bool ParseYamlTable(const string& config, YAML::Node& yamlRoot, string& errorMsg) {
    try {
        yamlRoot = YAML::Load(config);
    } catch (const YAML::ParserException& e) {
        errorMsg = "parse yaml failed: " + string(e.what());
        return false;
    } catch (const exception& e) {
        errorMsg = "unknown exception: " + string(e.what());
        return false;
    } catch (...) {
        errorMsg = "unknown error";
        return false;
    }
    return true;
}

Json::Value ParseScalar(const YAML::Node& node) {
    // yaml-cpp automatically discards quotes in quoted values,
    // so to differentiate strings and integer for purely-digits value,
    // we can tell from node.Tag(), which will return "!" for quoted values and "?" for others
    if (node.Tag() == "!") {
        return node.as<string>();
    }

    int i;
    if (YAML::convert<int>::decode(node, i))
        return Json::Value(i);

    double d;
    if (YAML::convert<double>::decode(node, d))
        return Json::Value(d);

    bool b;
    if (YAML::convert<bool>::decode(node, b))
        return Json::Value(b);

    string s;
    if (YAML::convert<string>::decode(node, s))
        return Json::Value(s);

    return Json::Value();
}

Json::Value ConvertYamlToJson(const YAML::Node& rootNode) {
    Json::Value result;
    switch (rootNode.Type()) {
        case YAML::NodeType::Scalar:
            return ParseScalar(rootNode);
        case YAML::NodeType::Sequence: {
            result = Json::Value(Json::arrayValue); // create an empty array
            for (const auto& node : rootNode) {
                result.append(ConvertYamlToJson(node));
            }
            break;
        }
        case YAML::NodeType::Map: {
            result = Json::Value(Json::objectValue); // create an empty object
            for (const auto& node : rootNode) {
                result[node.first.as<string>()] = ConvertYamlToJson(node.second);
            }
            break;
        }
        default:
            break;
    }
    return result;
}

} // namespace logtail
