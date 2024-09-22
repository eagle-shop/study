cmake_minimum_required(VERSION 3.16)
project(StudyOpencryptoki CXX)

add_library(StudyOpencryptoki study_opencryptoki.cpp)

target_include_directories(StudyOpencryptoki
  PRIVATE
)

target_link_libraries(StudyOpencryptoki
  PRIVATE
    opencryptoki
    pthread
)
