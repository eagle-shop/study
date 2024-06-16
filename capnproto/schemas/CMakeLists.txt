cmake_minimum_required(VERSION 3.16)
project(schemas CXX)

capnp_generate_cpp(studySources studyHeaders study.capnp)

add_library(schemas
  ${studySources}
)

target_include_directories(schemas
  PRIVATE
    ${CapnProto_DIR}/../../../include
)

set(generateHeaders ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
