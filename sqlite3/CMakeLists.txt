cmake_minimum_required(VERSION 3.16)
project(StudySqlite3 CXX)

add_library(StudySqlite3 study_sqlite3.cpp)

target_include_directories(StudySqlite3
  PRIVATE
)

target_compile_features(StudySqlite3
  PUBLIC
    cxx_std_17
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(StudySqlite3)

target_link_libraries(StudySqlite3
  PRIVATE
    sqlite3
    pthread
)
