cmake_minimum_required(VERSION 3.16)
project(study_libarchive CXX)

add_executable(study_libarchive main.cpp)

target_include_directories(study_libarchive
  PRIVATE
)

target_compile_features(study_libarchive
  PUBLIC
    cxx_std_20
)

include(${CMAKE_SOURCE_DIR}/options.cmake)
set_common_compile_options(study_libarchive)

target_link_libraries(study_libarchive
  PRIVATE
    archive
    pthread
)
