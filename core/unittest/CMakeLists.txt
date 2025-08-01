# Copyright 2022 iLogtail Authors
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

cmake_minimum_required(VERSION 3.22)
project(unittest_base)

if (LINUX)
    # Unittest should be able to visit private members
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")
endif ()

add_definitions(-DAPSARA_UNIT_TEST_MAIN)
set(NO_TCMALLOC TRUE)
add_definitions(-DLOGTAIL_NO_TC_MALLOC)
if (MSVC)
    add_definitions(-DNOMINMAX)
endif ()

macro(add_core_subdir)
    add_subdirectory(app_config)
    add_subdirectory(batch)
    add_subdirectory(checkpoint)
    add_subdirectory(common)
    add_subdirectory(compression)
    add_subdirectory(config)
    add_subdirectory(container_manager)
    add_subdirectory(controller)
    add_subdirectory(event)
    add_subdirectory(event_handler)
    add_subdirectory(file_source)
    add_subdirectory(flusher)
    add_subdirectory(input)
    add_subdirectory(metadata)
    add_subdirectory(log_pb)
    add_subdirectory(models)
    add_subdirectory(monitor)
    add_subdirectory(pipeline)
    add_subdirectory(instance_config)
    add_subdirectory(plugin)
    add_subdirectory(polling)
    add_subdirectory(processor)
    add_subdirectory(provider)
    add_subdirectory(queue)
    add_subdirectory(reader)
    add_subdirectory(sender)
    add_subdirectory(serializer)
    add_subdirectory(route)
    add_subdirectory(task_pipeline)
    if (LINUX)
        add_subdirectory(ebpf)
        add_subdirectory(host_monitor)
        add_subdirectory(forward)
        add_subdirectory(prometheus)
        if (ENABLE_ENTERPRISE)
            add_subdirectory(shennong)
        endif()
    endif()

endmacro()

macro(add_spl_subdir)
    if (LINUX)
        add_subdirectory(spl)
    endif ()    
endmacro()

macro(ut_link ut_link_withspl)
    input_link(${UT_BASE_TARGET})
    processor_link(${UT_BASE_TARGET} ${ut_link_withspl})
    flusher_link(${UT_BASE_TARGET})
    all_link(${UT_BASE_TARGET})
    common_link(${UT_BASE_TARGET})
    target_link_libraries(${UT_BASE_TARGET} provider_unittest_base)
    if (LINUX)
        add_dependencies(${UT_BASE_TARGET} install_coolbpf)
    endif()
endmacro()

set(SOURCE_FILES_CORE ${FRAMEWORK_SOURCE_FILES} ${PLUGIN_SOURCE_FILES_CORE})
set(SOURCE_FILES_CORE_WITHSPL ${SOURCE_FILES_CORE} ${PLUGIN_SOURCE_FILES_SPL})

# add provider
set(PROVIDER_BASE_TARGET "provider_unittest_base")
add_subdirectory("${PROVIDER_PATH}" ${CMAKE_BINARY_DIR}/${PROVIDER_BASE_TARGET})

if (UNIX)
    if (LINUX)
        if (WITHSPL)
            # add spl subdir
            set(UT_BASE_TARGET "unittest_base_withspl")
            add_library(${UT_BASE_TARGET} STATIC ${SOURCE_FILES_CORE_WITHSPL})
            target_compile_options(${UT_BASE_TARGET} PRIVATE -Werror)
            add_spl_subdir()
            set(UT_LINK_WITHSPL ON)
            ut_link(${UT_LINK_WITHSPL})
        endif()
        # add core subdir
        set(UT_BASE_TARGET "unittest_base")
        add_definitions(-D__EXCLUDE_SPL__)
        add_library(${UT_BASE_TARGET} SHARED ${SOURCE_FILES_CORE})
        target_compile_options(${UT_BASE_TARGET} PRIVATE -Werror)
        add_core_subdir()
        set(UT_LINK_WITHSPL OFF)
        ut_link(${UT_LINK_WITHSPL})
    else ()
        # add core subdir
        set(UT_BASE_TARGET "unittest_base")
        add_library(${UT_BASE_TARGET} STATIC ${SOURCE_FILES_CORE})
        target_compile_options(${UT_BASE_TARGET} PRIVATE -Werror)
        add_core_subdir()
        set(UT_LINK_WITHSPL OFF)
        ut_link(${UT_LINK_WITHSPL})
    endif ()
elseif (MSVC)
    # add core subdir
    set(UT_BASE_TARGET "unittest_base")
    add_library(${UT_BASE_TARGET} STATIC ${SOURCE_FILES_CORE})
    # For MSVC, use /W4 for a high level of warnings, and treat warnings as errors.
    target_compile_options(${UT_BASE_TARGET} PRIVATE /W4)
    add_core_subdir()
    set(UT_LINK_WITHSPL OFF)
    ut_link(${UT_LINK_WITHSPL})
endif()
