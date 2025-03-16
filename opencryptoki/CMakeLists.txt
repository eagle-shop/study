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

target_compile_features(StudyOpencryptoki
  PUBLIC
    cxx_std_20
)

target_compile_options(StudyOpencryptoki
  PUBLIC
    -Wall
    -Wextra
    -Werror
)
