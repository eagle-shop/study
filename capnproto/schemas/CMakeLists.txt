cmake_minimum_required(VERSION 3.16)
project(schemas CXX)

capnp_generate_cpp(studySources studyHeaders
  es_util.capnp
  common.capnp
  study.capnp
)

add_library(schemas
  ${studySources}
)

target_include_directories(schemas
  PRIVATE
    ${CapnProto_INCLUDE_DIRS}
)

target_compile_features(schemas
  PUBLIC
    cxx_std_17
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(schemas)
