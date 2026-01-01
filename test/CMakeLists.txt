cmake_minimum_required(VERSION 3.16)
project(Test CXX)

add_executable(Test
  main.cpp
  study_opencryptoki_test.cpp
  study_sqlite3_test.cpp
  study_capnproto_server_test.cpp
)

target_include_directories(Test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/opencryptoki
    ${CMAKE_SOURCE_DIR}/sqlite3
    ${CMAKE_SOURCE_DIR}/capnproto/server
    ${CMAKE_SOURCE_DIR}/capnproto/log
    ${CMAKE_BINARY_DIR}/capnproto/schemas
)

target_compile_features(Test
  PUBLIC
    cxx_std_17
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(Test)

target_link_libraries(Test
  PRIVATE
    StudyOpencryptoki
    StudySqlite3
    gtest
    pthread
    server
    schemas
    log
    CapnProto::capnp-rpc
)

add_dependencies(Test schemas)
