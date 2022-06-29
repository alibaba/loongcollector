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

#ifndef __SLS_ILOGTAIL_ENCODING_CONVERTER_H__
#define __SLS_ILOGTAIL_ENCODING_CONVERTER_H__

#include <vector>
#include <string>
#include <cstddef>

namespace logtail {
enum FileEncoding { ENCODING_UTF8, ENCODING_GBK };

class EncodingConverter {
private:
    EncodingConverter();
    ~EncodingConverter();

public:
    // notice: this is not thread-safe !
    static EncodingConverter* GetInstance() {
        static EncodingConverter* ptr = new EncodingConverter();
        return ptr;
    }

    // ConvertGbk2Utf8 converts src (in GBK) to des (in UTF-8).
    // @des: output param to store the result. If the return value is true, the memory
    //   of @des should be released by caller (with delete[]).
    // @return: return true/false to indicate if the converting succeeds or not.
    //   Output params follow: @des == NULL <=> *@desLength == 0.
    //
    // Different platforms have different implementations:
    // - For Linux, ConvertGbk2Utf8 converts line by line according to @linePosVec.
    //   If there is error happened during converting, corresponding line will be copied
    //   to @des without converting.
    // - For Windows, ConvertGbk2Utf8 converts whole @src, if any errors happened,
    //   @des will be set to NULL (ignore @linePosVec).
    bool
    ConvertGbk2Utf8(char* src, size_t* srcLength, char*& des, size_t* desLength, const std::vector<size_t>& linePosVec);

    // FromUTF8ToACP converts @s encoded in UTF8 to ACP.
    // @return ACP string if convert successfully, otherwise @s will be returned.
    std::string FromUTF8ToACP(const std::string& s);

    // FromACPToUTF8 converts @s encoded in ACP (locale) to UTF8.
    std::string FromACPToUTF8(const std::string& s);
};

} // namespace logtail

#endif
