cmake_minimum_required(VERSION 3.16)
project(study_capnp CXX)

set(generateHeaders "")

find_package(CapnProto CONFIG REQUIRED)

add_subdirectory(schemas)
add_subdirectory(server)
add_subdirectory(client)

add_executable(study_capnp main.cpp)

target_include_directories(study_capnp
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${generateHeaders}
)

target_link_libraries(study_capnp
  PRIVATE
    server
    client
    schemas
    CapnProto::capnp-rpc
    pthread
)

add_dependencies(study_capnp schemas)
add_dependencies(server schemas)
add_dependencies(client schemas)
