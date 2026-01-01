cmake_minimum_required(VERSION 3.16)
project(client CXX)

add_library(client client.cpp)

target_include_directories(client
  PRIVATE
    ../log
    ${CapnProto_INCLUDE_DIRS}
    ${generateHeaders}
)

target_compile_features(client
  PUBLIC
    cxx_std_17
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(client)

add_dependencies(client schemas)
