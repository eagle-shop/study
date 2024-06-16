cmake_minimum_required(VERSION 3.16)
project(study CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-Werror")

find_package(CapnProto CONFIG REQUIRED)

add_subdirectory(capnproto)
