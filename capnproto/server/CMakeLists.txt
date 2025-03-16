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
    cxx_std_20
)

target_compile_options(server
  PUBLIC
    -Wall
    -Wextra
    -Werror
)

add_dependencies(server schemas)
