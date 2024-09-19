cmake_minimum_required(VERSION 3.16)
project(study_opencryptoki CXX)

add_executable(study_opencryptoki main.cpp)

target_include_directories(study_opencryptoki
  PRIVATE
)

target_link_libraries(study_opencryptoki
  PRIVATE
    opencryptoki
    pthread
)
