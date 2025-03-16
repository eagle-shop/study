cmake_minimum_required(VERSION 3.16)
project(study CXX)

find_package(CapnProto CONFIG REQUIRED)

add_subdirectory(capnproto)
add_subdirectory(libarchive)
add_subdirectory(opencryptoki)
add_subdirectory(sqlite3)
add_subdirectory(test)
