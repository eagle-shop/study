cmake_minimum_required(VERSION 3.16)
project(server CXX)

add_library(server server.cpp)

target_include_directories(server
  PRIVATE
    ${CapnProto_DIR}/../../../include
    ${generateHeaders}
)
