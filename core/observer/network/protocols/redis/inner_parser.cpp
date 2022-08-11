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

#include <iostream>
#include <string>

#include "inner_parser.h"
namespace logtail {

SlsStringPiece RedisParser::readUtilNewLine() {
    const char* s = readChar();
    const char* ch = s;
    const char* lastCh = s;
    int counter = 1;
    while (true) {
        ch++;
        counter++;
        positionCommit(1);
        if (isParseFail) {
            SlsStringPiece empty;
            return empty;
        }

        if ((*lastCh) == '\r' && (*ch) == '\n') {
            break;
        }
        lastCh = ch;
    }
    SlsStringPiece val(s, counter - 2);
    return val;
}

void RedisParser::readData(std::vector<SlsStringPiece>& data) {
    const char* ch = readChar();
    switch (*ch) {
        case '+': // + 字符串 \r\n
        {
            auto s = readUtilNewLine();
            // std::cout << s1.ToString() << std::endl;
            data.push_back(s);
        } break;
        case '-': // - 错误前缀 错误信息 \r\n
        {
            auto s = readUtilNewLine();
            // std::cout << s1.ToString() << std::endl;
            data.push_back(s);
            redisData.isError = true;
        } break;
        case ':': // : 数字 \r\n
        {
            auto s = readUtilNewLine();
            // std::cout << s1.ToString() << std::endl;
            data.push_back(s);
        } break;
        case '$': // $ 字符串的长度 \r\n 字符串 \r\n
        {
            readUtilNewLine();
            auto s2 = readUtilNewLine();
            data.push_back(s2);
            break;
        }
        case '*': // * 数组元素个数 \r\n 其他所有类型 (结尾不需要\r\n)
        {
            auto s = readUtilNewLine();
            int cnt = std::stoi(s.ToString());
            int max = cnt > 1 ? 1 : cnt;
            for (int i = 0; i < max; i++) {
                readData(data);
            }
            break;
        }
        default:
            break;
    }
}

void RedisParser::parse() {
    readData(redisData.data);
}

void RedisParser::print() {
    for (auto v : redisData.data) {
        std::cout << v.ToString() << " ";
    }

    std::cout << std::endl;
}

} // end of namespace logtail