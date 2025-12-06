cmake_minimum_required(VERSION 3.16)
project(server CXX)

add_library(server server.cpp)

target_include_directories(server
  PRIVATE
    ../log
    ${CapnProto_DIR}/../../../include
    ${generateHeaders}
)

target_compile_features(server
  PUBLIC
    cxx_std_17
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(server)

add_dependencies(server schemas)
