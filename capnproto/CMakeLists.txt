cmake_minimum_required(VERSION 3.16)
project(study_capnp CXX)

set(generateHeaders "")

find_package(CapnProto CONFIG REQUIRED)

add_subdirectory(schemas)
add_subdirectory(log)
add_subdirectory(server)
add_subdirectory(client)

add_executable(study_capnp main.cpp)

target_include_directories(study_capnp
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/log
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${generateHeaders}
)


target_compile_features(study_capnp
  PUBLIC
    cxx_std_20
)

target_compile_options(study_capnp
  PUBLIC
    -Wall
    -Wextra
    -Werror
)

target_link_libraries(study_capnp
  PRIVATE
    server
    client
    schemas
    log
    CapnProto::capnp-rpc
    pthread
)
