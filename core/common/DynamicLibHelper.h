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

#include <string>
#include <cstdlib>

namespace logtail {


class DynamicLibLoader;

// setns function first appear in kernel 3 and glibc 1.14.
typedef int (*glibc_setns_func)(int __fd, int __nstype);

// For logtail running with centos 6, use dlopen to load some glibc functions.
namespace glibc {
    // setns function first appear in kernel 3 and glibc 1.14.
    typedef int (*glibc_setns_func)(int __fd, int __nstype);
    extern glibc_setns_func g_setns_func;
    extern DynamicLibLoader* g_loader;
    bool LoadGlibcFunc();

} // namespace glibc

class DynamicLibLoader {
    void* mLibPtr = nullptr;

    std::string GetError();

public:
    static void CloseLib(void* libPtr);

    // Release releases the ownership of @mLibPtr to caller.
    void* Release();

    ~DynamicLibLoader();

    // LoadDynLib loads dynamic library named @libName from current working dir.
    // For linux, the so name is 'lib+@libName.so'.
    // For Windows, the dll name is '@libName.dll'.
    // @return a non-NULL ptr to indicate lib handle, otherwise nullptr is returned.
    bool LoadDynLib(const std::string& libName,
                    std::string& error,
                    const std::string dlPrefix = "",
                    const std::string dlSuffix = "");

    // LoadMethod loads method named @methodName from opened lib.
    // If error is found, @error param will be set, otherwise empty.
    void* LoadMethod(const std::string& methodName, std::string& error);
};


} // namespace logtail