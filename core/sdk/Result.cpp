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

#include "Result.h"
#include "Exception.h"

namespace logtail {

namespace sdk {

    using namespace std;

    /************************ common method ***********************/
    /************************ json method *************************/
    void ExtractJsonResult(const string& response, rapidjson::Document& document) {
        document.Parse(response.c_str());
        if (document.HasParseError()) {
            throw JsonException("ParseException", "Fail to parse from json string");
        }
    }

    void JsonMemberCheck(const rapidjson::Value& value, const char* name) {
        if (!value.IsObject()) {
            throw JsonException("InvalidObjectException", "response is not valid JSON object");
        }
        if (!value.HasMember(name)) {
            throw JsonException("NoMemberException", string("Member ") + name + " does not exist");
        }
    }

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, int32_t& number) {
        JsonMemberCheck(value, name);
        if (value[name].IsInt()) {
            number = value[name].GetInt();
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not int type");
        }
    }

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, uint32_t& number) {
        JsonMemberCheck(value, name);
        if (value[name].IsUint()) {
            number = value[name].GetUint();
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not uint type");
        }
    }

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, int64_t& number) {
        JsonMemberCheck(value, name);
        if (value[name].IsInt64()) {
            number = value[name].GetInt64();
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not int type");
        }
    }

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, uint64_t& number) {
        JsonMemberCheck(value, name);
        if (value[name].IsUint64()) {
            number = value[name].GetUint64();
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not uint type");
        }
    }

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, bool& boolean) {
        JsonMemberCheck(value, name);
        if (value[name].IsBool()) {
            boolean = value[name].GetBool();
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not boolean type");
        }
    }

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, string& dst) {
        JsonMemberCheck(value, name);
        if (value[name].IsString()) {
            dst = value[name].GetString();
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not string type");
        }
    }

    const rapidjson::Value& GetJsonValue(const rapidjson::Value& value, const char* name) {
        JsonMemberCheck(value, name);
        if (value[name].IsObject() || value[name].IsArray()) {
            return value[name];
        } else {
            throw JsonException("ValueTypeException", string("Member ") + name + " is not json value type");
        }
    }


    void ErrorCheck(const string& response, const string& requestId, const int32_t httpCode) {
        rapidjson::Document document;
        try {
            ExtractJsonResult(response, document);

            string errorCode;
            ExtractJsonResult(document, LOG_ERROR_CODE, errorCode);

            string errorMessage;
            ExtractJsonResult(document, LOG_ERROR_MESSAGE, errorMessage);

            throw LOGException(errorCode, errorMessage, requestId, httpCode);
        } catch (JsonException& e) {
            if (httpCode >= 500) {
                throw LOGException(LOGE_INTERNAL_SERVER_ERROR, response, requestId, httpCode);
            } else {
                throw LOGException(LOGE_BAD_RESPONSE, string("Unextractable error:") + response, requestId, httpCode);
            }
        }
    }

} // namespace sdk
} // namespace logtail
