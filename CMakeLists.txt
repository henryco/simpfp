cmake_minimum_required(VERSION 3.30)
project(simpfp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(simpfp INTERFACE)
target_include_directories(simpfp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)