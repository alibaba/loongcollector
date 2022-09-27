cmake_minimum_required(VERSION 2.9)

#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_FLAGS   "-g")
#set(CMAKE_CXX_FLAGS   "-Wall")

project(mysqlparser)

# include network dir
include_directories("../../../")

file(GLOB LIB_SOURCE_FILES parser.cpp inner_parser.cpp *.h)
append_source_files(LIB_SOURCE_FILES)
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCE_FILES})

