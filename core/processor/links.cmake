
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
# This file is used to link external source files in processor directory

macro(processor_link target_name)
    link_re2(${target_name})
    if (LINUX AND NOT WITHOUTSPL)
        link_spl(${target_name})
    endif ()
    if (LINUX AND NOT WITHOUTSPL)
        target_link_libraries(${target_name} spl)
    endif ()
    link_ssl(${target_name}) # must after link_spl
    link_crypto(${target_name}) # must after link_spl
endmacro()
