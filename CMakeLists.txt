cmake_minimum_required(VERSION 3.11)
include_guard(DIRECTORY)

project(
  poly-standalone
  VERSION 1.0.0
  LANGUAGES CXX
)

option(POLY_BUILD_EXAMPLE "Build the example driver." OFF)

add_library(poly-standalone INTERFACE)
add_library(poly::standalone ALIAS poly-standalone)
target_include_directories(poly-standalone INTERFACE include)

if(POLY_BUILD_EXAMPLE)
  add_executable(poly Driver.cpp)
  target_link_libraries(poly poly::standalone)
endif()
