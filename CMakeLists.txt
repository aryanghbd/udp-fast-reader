cmake_minimum_required(VERSION 3.16)
project(udp_fast_reader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(udpreader STATIC
  src/url.cpp
  src/udp_receiver.cpp
)
target_include_directories(udpreader PUBLIC src)
target_compile_options(udpreader PRIVATE -Wall -Wextra -Wpedantic)

add_executable(udp-fast-reader src/main.cpp)
target_link_libraries(udp-fast-reader PRIVATE udpreader)
target_compile_options(udp-fast-reader PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(udp-tests tests/test_main.cpp)
target_link_libraries(udp-tests PRIVATE udpreader)
add_test(NAME udp-tests COMMAND udp-tests)
