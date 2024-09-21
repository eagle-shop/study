cmake_minimum_required(VERSION 3.16)
project(study_sqlite3 CXX)

add_executable(study_sqlite3 main.cpp)

target_include_directories(study_sqlite3
  PRIVATE
)

target_link_libraries(study_sqlite3
  PRIVATE
    sqlite3
    pthread
)
