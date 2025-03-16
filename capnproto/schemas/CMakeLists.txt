cmake_minimum_required(VERSION 3.16)
project(schemas CXX)

capnp_generate_cpp(studySources studyHeaders
  stream.capnp
  study.capnp
)

add_library(schemas
  ${studySources}
)

target_include_directories(schemas
  PRIVATE
    ${CapnProto_DIR}/../../../include
)

target_compile_features(schemas
  PUBLIC
    cxx_std_20
)

target_compile_options(schemas
  PUBLIC
    -Wall
    -Wextra
    -Werror
)

set(generateHeaders ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
