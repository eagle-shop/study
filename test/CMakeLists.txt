cmake_minimum_required(VERSION 3.16)
project(Test CXX)

add_executable(Test
  main.cpp
  study_opencryptoki_test.cpp
  study_sqlite3_test.cpp
)

target_include_directories(Test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/opencryptoki
    ${CMAKE_SOURCE_DIR}/sqlite3
)

target_link_libraries(Test
  PRIVATE
    StudyOpencryptoki
    StudySqlite3
    gtest
    pthread
)

add_dependencies(Test StudyOpencryptoki)
add_dependencies(Test StudySqlite3)
