cmake_minimum_required(VERSION 3.16)
project(study_capnp CXX)

find_package(CapnProto CONFIG REQUIRED)

add_subdirectory(schemas)
add_subdirectory(log)
add_subdirectory(server)
