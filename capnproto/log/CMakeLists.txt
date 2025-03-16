cmake_minimum_required(VERSION 3.16)
project(server CXX)

add_library(log log.cpp)

target_include_directories(log
  PRIVATE
)

target_compile_features(log
  PUBLIC
    cxx_std_20
)

target_compile_options(log
  PUBLIC
    -Wall
    -Wextra
    -Werror
)
