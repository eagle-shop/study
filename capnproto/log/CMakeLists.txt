cmake_minimum_required(VERSION 3.16)
project(log CXX)

add_library(log log.cpp)

target_include_directories(log
  PRIVATE
)

target_compile_features(log
  PUBLIC
    cxx_std_17
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(log)
