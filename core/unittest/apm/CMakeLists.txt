cmake_minimum_required(VERSION 3.22)
project(apm_unittest)

include(GoogleTest)

function(add_unittest target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${UT_BASE_TARGET})
    gtest_discover_tests(${target_name})
endfunction()

add_unittest(attach_manager_unittest AttachManagerUnittest.cpp)
add_unittest(package_manager_unittest PackageManagerUnittest.cpp)
add_unittest(apm_inject_config_unittest ApmInjectConfigUnittest.cpp)
