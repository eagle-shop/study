cmake_minimum_required(VERSION 3.16)
project(client CXX)

add_library(client client.cpp)

target_include_directories(client
  PRIVATE
    ../log
    ${CapnProto_DIR}/../../../include
    ${generateHeaders}
)

target_compile_features(client
  PUBLIC
    cxx_std_20
)

target_compile_options(client
  PUBLIC
    -Wall
    -Wextra
    -Werror
)

add_dependencies(client schemas)
