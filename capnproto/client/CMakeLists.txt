cmake_minimum_required(VERSION 3.16)
project(client CXX)

add_library(client client.cpp)

target_include_directories(client
  PRIVATE
    ${CapnProto_DIR}/../../../include
    ${generateHeaders}
)
