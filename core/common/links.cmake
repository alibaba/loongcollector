
# Copyright 2024 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# This file is used to link external source files in common directory

macro(common_link target_name)
    link_jsoncpp(${target_name})
    link_yamlcpp(${target_name})
    link_boost(${target_name})
    link_gflags(${target_name})
    link_lz4(${target_name})
    link_zlib(${target_name})
    link_zstd(${target_name})
    link_unwind(${target_name})
    if (ENABLE_ADDRESS_SANITIZER)
        link_asan(${target_name})
    endif()
    if (UNIX)
        link_uuid(${target_name})
        if (LINUX)
            target_link_libraries(${target_name} pthread)
        endif ()
    elseif (MSVC)
        target_link_libraries(${target_name} "ws2_32.lib")
        target_link_libraries(${target_name} "Rpcrt4.lib")
        target_link_libraries(${target_name} "Shlwapi.lib")
    endif ()
endmacro()