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

#include "EncodingConverter.h"
#include "LogtailAlarm.h"
#include "logger/Logger.h"
#if defined(__linux__)
#include <iconv.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#endif

namespace logtail {

#if defined(__linux__)
static iconv_t mGbk2Utf8Cd = (iconv_t)-1;
#endif

EncodingConverter::EncodingConverter() {
#if defined(__linux__)
    mGbk2Utf8Cd = iconv_open("UTF-8", "GBK");
    if (mGbk2Utf8Cd == (iconv_t)(-1))
        LOG_ERROR(sLogger, ("create Gbk2Utf8 iconv descriptor fail, errno", strerror(errno)));
    else
        iconv(mGbk2Utf8Cd, NULL, NULL, NULL, NULL);
#endif
}

EncodingConverter::~EncodingConverter() {
#if defined(__linux__)
    if (mGbk2Utf8Cd != (iconv_t)(-1))
        iconv_close(mGbk2Utf8Cd);
#endif
}

// TODO: Refactor it, do not use the output params to do calculations, set them before return.
size_t EncodingConverter::ConvertGbk2Utf8(
    const char* src, size_t* srcLength, char* desOut, size_t desLength, const std::vector<long>& linePosVec) const {
#if defined(__linux__)
    if (src == NULL || *srcLength == 0 || mGbk2Utf8Cd == (iconv_t)(-1)) {
        LOG_ERROR(sLogger, ("invalid iconv descriptor fail or invalid buffer pointer, cd", mGbk2Utf8Cd));
        return 0;
    }
    size_t maxRequire = *srcLength * 2;
    if (desOut == nullptr) {
        return maxRequire;
    }
    if (desLength < maxRequire + 1) {
        return 0;
    }
    char* des = desOut;
    des[*srcLength * 2] = '\0';
    const char* originSrc = src;
    char* originDes = des;
    size_t beginIndex = 0;
    size_t endIndex = *srcLength;
    size_t destIndex = 0;
    size_t maxDestSize = desLength;
    for (size_t i = 0; i < linePosVec.size(); ++i) {
        endIndex = linePosVec[i];
        src = originSrc + beginIndex;
        des = originDes + destIndex;
        // include '\n'
        *srcLength = endIndex - beginIndex + 1;
        desLength = maxDestSize - destIndex;
        size_t ret = iconv(mGbk2Utf8Cd, const_cast<char**>(&src), srcLength, &des, &desLength);
        if (ret == (size_t)(-1)) {
            LOG_ERROR(sLogger, ("convert GBK to UTF8 fail, errno", strerror(errno)));
            iconv(mGbk2Utf8Cd, NULL, NULL, NULL, NULL); // Clear status.
            LogtailAlarm::GetInstance()->SendAlarm(ENCODING_CONVERT_ALARM, "convert GBK to UTF8 fail");
            // use memcpy
            memcpy(originDes + destIndex, originSrc + beginIndex, endIndex - beginIndex + 1);
            destIndex += endIndex - beginIndex + 1;
        } else {
            destIndex = des - originDes;
        }
        beginIndex = endIndex + 1;
    }
    return destIndex;

#elif defined(_MSC_VER)
    int wcLen = MultiByteToWideChar(CP_ACP, 0, src, *srcLength, NULL, 0);
    if (wcLen == 0) {
        LOG_ERROR(sLogger,
                  ("convert GBK to UTF8 fail, MultiByteToWideChar error", GetLastError())("sample",
                                                                                          std::string(src, 0, 1024)));
        return 0;
    }
    wchar_t* wszUtf8 = new wchar_t[wcLen + 1];
    if (MultiByteToWideChar(CP_ACP, 0, src, *srcLength, (LPWSTR)wszUtf8, wcLen) == 0) {
        LOG_ERROR(sLogger,
                  ("convert GBK to UTF8 fail, MultiByteToWideChar error", GetLastError())("sample",
                                                                                          std::string(src, 0, 1024)));
        delete[] wszUtf8;
        return 0;
    }
    wszUtf8[wcLen] = '\0';
    if (desOut == nullptr) {
        int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)wszUtf8, wcLen, NULL, 0, NULL, NULL);
        if (len == 0) {
            LOG_ERROR(sLogger,
                      ("convert GBK to UTF8 fail, WideCharToMultiByte error",
                       GetLastError())("sample", std::string(src, 0, 1024)));
            delete[] wszUtf8;
            return 0;
        }
        return len;
    }
    char* des = desOut;
    int len = desLength - 1;
    des[len] = '\0';
    int outLen = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)wszUtf8, wcLen, des, len, NULL, NULL);
    if ((outLen) == 0) {
            LOG_ERROR(sLogger,
                      ("convert GBK to UTF8 fail, WideCharToMultiByte error",
                       GetLastError())("sample", std::string(src, 0, 1024)));
            delete[] wszUtf8;
            delete[] des;
            return 0;
        }
    delete[] wszUtf8;
    return outLen;
#endif
}

#if defined(_MSC_VER)
std::string EncodingConverter::FromUTF8ToACP(const std::string& s) const {
    auto input = s.c_str();
    auto requiredLen = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
    if (requiredLen <= 0) {
        LOG_ERROR(sLogger,
                  ("convert UTF8 to ACP fail, MultiByteToWideChar error", GetLastError())("source", s.substr(0, 1024)));
        return s;
    }
    wchar_t* wszUTF8 = new wchar_t[requiredLen];
    auto len = MultiByteToWideChar(CP_UTF8, 0, input, -1, wszUTF8, requiredLen);
    if (len <= 0) {
        LOG_ERROR(sLogger,
                  ("convert UTF8 to ACP fail, MultiByteToWideChar error", GetLastError())("source", s.substr(0, 1024)));
        delete[] wszUTF8;
        return s;
    }
    requiredLen = WideCharToMultiByte(CP_ACP, 0, wszUTF8, -1, NULL, 0, NULL, NULL);
    if (requiredLen <= 0) {
        LOG_ERROR(sLogger,
                  ("convert UTF8 to ACP fail, WideCharToMultiByte error", GetLastError())("source", s.substr(0, 1024)));
        delete[] wszUTF8;
        return s;
    }
    auto acpStr = new char[requiredLen];
    len = WideCharToMultiByte(CP_ACP, 0, wszUTF8, -1, acpStr, requiredLen, NULL, NULL);
    if (len <= 0) {
        LOG_ERROR(sLogger,
                  ("convert UTF8 to ACP fail, WideCharToMultiByte error", GetLastError())("source", s.substr(0, 1024)));
        delete[] wszUTF8;
        delete[] acpStr;
        return s;
    }
    delete[] wszUTF8;
    std::string ret(acpStr, requiredLen - 1);
    delete[] acpStr;
    return ret;
}

std::string EncodingConverter::FromACPToUTF8(const std::string& s) const {
    if (s.empty())
        return s;

    auto input = const_cast<char*>(s.c_str());
    auto inputLen = s.length();
    std::vector<long> ignore;

    size_t outputLen = ConvertGbk2Utf8(input, &inputLen, nullptr, 0, ignore);
    if (outputLen == 0) {
        LOG_WARNING(sLogger, ("Convert ACP to UTF8 failed", s.substr(0, 1024)));
        return s;
    }
    std::string ret;
    ret.resize(outputLen + 1);
    if (!ConvertGbk2Utf8(input, &inputLen, const_cast<char*>(ret.data()), outputLen + 1, ignore)) {
        LOG_WARNING(sLogger, ("Convert ACP to UTF8 failed", s.substr(0, 1024)));
        return s;
    }
    ret.resize(outputLen);
    ret.c_str();
    return ret;
}
#endif

} // namespace logtail
