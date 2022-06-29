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
#if defined(_MSC_VER)
#include <Winsock2.h>
#endif
#include <cstdlib>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include "logger/Logger.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/HashUtil.h"
#include "common/TimeUtil.h"

namespace bfs = boost::filesystem;

#define APSARA_TEST_TRUE(condition) \
    do { \
        bool _rst_inner = (condition); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_TRUE", _rst_inner)("condition", #condition)); \
        EXPECT_TRUE(_rst_inner); \
    } while (0)

#define APSARA_TEST_TRUE_DESC(condition, desc) \
    do { \
        bool _rst_inner = (condition); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_TRUE", _rst_inner)("condition", #condition)("DESC", desc)); \
        EXPECT_TRUE(_rst_inner); \
    } while (0)

#define APSARA_TEST_TRUE_FATAL(condition) \
    do { \
        bool _rst_inner = (condition); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_TRUE_FATAL", _rst_inner)("condition", #condition)); \
        ASSERT_TRUE(_rst_inner); \
    } while (0)

#define APSARA_TEST_FALSE(condition) \
    do { \
        bool _rst_inner = (condition); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_FALSE", _rst_inner)("condition", #condition)); \
        EXPECT_FALSE(_rst_inner); \
    } while (0)

#define APSARA_TEST_FALSE_DESC(condition, desc) \
    do { \
        bool _rst_inner = (condition); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_FALSE", _rst_inner)("condition", #condition)("DESC", desc)); \
        EXPECT_FALSE(_rst_inner); \
    } while (0)

#define APSARA_TEST_FALSE_FATAL(condition) \
    do { \
        bool _rst_inner = (condition); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_FALSE_FATAL", _rst_inner)("condition", #condition)); \
        ASSERT_FALSE(_rst_inner); \
    } while (0)

#define APSARA_TEST_EQUAL(val1, val2) \
    do { \
        auto _v1_inner = val1; \
        auto _v2_inner = val2; \
        auto _rst_inner = ::testing::internal::EqHelper<GTEST_IS_NULL_LITERAL_(_v1_inner)>::Compare( \
            #val1, #val2, _v1_inner, _v2_inner); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_EQUAL", (bool)_rst_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_EQ(_v1_inner, _v2_inner); \
    } while (0)

#define APSARA_TEST_EQUAL_DESC(val1, val2, desc) \
    do { \
        auto _v1_inner = val1; \
        auto _v2_inner = val2; \
        auto _rst_inner = ::testing::internal::EqHelper<GTEST_IS_NULL_LITERAL_(_v1_inner)>::Compare( \
            #val1, #val2, _v1_inner, _v2_inner); \
        APSARA_LOG_INFO(sLogger, \
                        ("APSARA_TEST_EQUAL", (bool)_rst_inner)("DESC", desc)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_EQ(_v1_inner, _v2_inner); \
    } while (0)

#define APSARA_TEST_EQUAL_FATAL(val1, val2) \
    do { \
        auto _v1_inner = val1; \
        auto _v2_inner = val2; \
        auto _rst_inner = ::testing::internal::EqHelper<GTEST_IS_NULL_LITERAL_(_v1_inner)>::Compare( \
            #val1, #val2, _v1_inner, _v2_inner); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_EQUAL_FATAL", (bool)_rst_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        ASSERT_EQ(_v1_inner, _v2_inner); \
    } while (0)

#define APSARA_TEST_NOT_EQUAL_DESC(val1, val2, desc) \
    do { \
        auto _v1_inner = val1; \
        auto _v2_inner = val2; \
        auto _rst_inner = ::testing::internal::EqHelper<GTEST_IS_NULL_LITERAL_(_v1_inner)>::Compare( \
            #val1, #val2, _v1_inner, _v2_inner); \
        APSARA_LOG_INFO(sLogger, \
                        ("APSARA_TEST_NOT_EQUAL", !_rst_inner)("DESC", desc)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_NE(_v1_inner, _v2_inner); \
    } while (0)

#define APSARA_TEST_NOT_EQUAL(val1, val2) \
    do { \
        auto _v1_inner = val1; \
        auto _v2_inner = val2; \
        auto _rst_inner = ::testing::internal::EqHelper<GTEST_IS_NULL_LITERAL_(_v1_inner)>::Compare( \
            #val1, #val2, _v1_inner, _v2_inner); \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_NOT_EQUAL", !_rst_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_NE(_v1_inner, _v2_inner); \
    } while (0)

#define APSARA_TEST_NOT_EQUAL_FATAL(val1, val2) \
    do { \
        auto _v1_inner = val1; \
        auto _v2_inner = val2; \
        auto _rst_inner = ::testing::internal::EqHelper<GTEST_IS_NULL_LITERAL_(_v1_inner)>::Compare( \
            #val1, #val2, _v1_inner, _v2_inner); \
        APSARA_LOG_INFO(sLogger, \
                        ("APSARA_TEST_NOT_EQUAL_FATAL", (bool)_rst_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        ASSERT_NE(_v1_inner, _v2_inner); \
    } while (0)

#define APSARA_TEST_STREQ(val1, val2) \
    do { \
        std::string _v1_inner = val1; \
        std::string _v2_inner = val2; \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_STREQ", _v1_inner == _v2_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_STREQ(_v1_inner.c_str(), _v2_inner.c_str()); \
    } while (0)

#define APSARA_TEST_STREQ_DESC(val1, val2, desc) \
    do { \
        std::string _v1_inner = val1; \
        std::string _v2_inner = val2; \
        APSARA_LOG_INFO( \
            sLogger, ("APSARA_TEST_STREQ", _v1_inner == _v2_inner)("DESC", desc)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_STREQ(_v1_inner.c_str(), _v2_inner.c_str()); \
    } while (0)

#define APSARA_TEST_STREQ_FATAL(val1, val2) \
    do { \
        std::string _v1_inner = val1; \
        std::string _v2_inner = val2; \
        APSARA_LOG_INFO(sLogger, \
                        ("APSARA_TEST_STREQ_FATAL", _v1_inner == _v2_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        ASSERT_STREQ(_v1_inner.c_str(), _v2_inner.c_str()); \
    } while (0)

#define APSARA_TEST_STRNE_DESC(val1, val2, desc) \
    do { \
        std::string _v1_inner = val1; \
        std::string _v2_inner = val2; \
        APSARA_LOG_INFO(sLogger, ("APSARA_TEST_STRNE", _v1_inner != _v2_inner)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_STRNE(_v1_inner.c_str(), _v2_inner.c_str()); \
    } while (0)

#define APSARA_TEST_STRNE(val1, val2) \
    do { \
        std::string _v1_inner = val1; \
        std::string _v2_inner = val2; \
        APSARA_LOG_INFO( \
            sLogger, ("APSARA_TEST_STRNE", _v1_inner != _v2_inner)("DESC", desc)(#val1, _v1_inner)(#val2, _v2_inner)); \
        EXPECT_STRNE(_v1_inner.c_str(), _v2_inner.c_str()); \
    } while (0)

#define APSARA_TEST_STRNE_FATAL(val1, val2) \
    do { \
        std::string _v1_inner = val1; \
        std::string _v2_inner = val2; \
    APSARA_LOG_INFO(sLogger, ("APSARA_TEST_STRNE_FATAL", _v1_inner != _v2_inner) != 0) (#val1, _v1_inner) (#val2, _v2_inner)); \
        ASSERT_STRNE(_v1_inner.c_str(), _v2_inner.c_str()); \
    } while (0)

#define APSARA_UNIT_TEST_CASE(suite, case, id) \
    TEST_F(suite, case) { \
        case();                         \
    }

#define UNIT_TEST_CASE(suite, case) APSARA_UNIT_TEST_CASE(suite, case, 0)

#define UNIT_TEST_MAIN \
    int main(int argc, char** argv) { \
        logtail::Logger::Instance().InitGlobalLoggers(); \
        ::testing::InitGoogleTest(&argc, argv); \
        return RUN_ALL_TESTS(); \
    }

void InitUnittestMain() {
    logtail::Logger::Instance().InitGlobalLoggers();

#if defined(_MSC_VER)
    // Initialize Winsock.
    int iResult;
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        LOG_FATAL(sLogger, ("Initialize Winsock failed", iResult));
        return;
    }
#endif
}

#ifdef ENABLE_COMPATIBLE_MODE
extern "C" {
#include <string.h>
asm(".symver memcpy, memcpy@GLIBC_2.2.5");
void* __wrap_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}
}
#endif

void SetEnv(const char* key, const char* value) {
#if defined(__linux__)
    setenv(key, value, 1);
#elif defined(_MSC_VER)
    _putenv_s(key, value);
#endif
}

void UnsetEnv(const char* key) {
#if defined(__linux__)
    unsetenv(key);
#elif defined(_MSC_VER)
    _putenv_s(key, "");
#endif
}
