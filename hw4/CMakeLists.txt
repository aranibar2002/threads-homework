cmake_minimum_required(VERSION 3.16)

project(ipc_mpsc_queue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(producer_demo producer_demo.cpp)
add_executable(consumer_demo consumer_demo.cpp)
add_executable(cleanup_demo cleanup_demo.cpp)

target_compile_options(producer_demo PRIVATE -O2 -Wall -Wextra -Wpedantic)
target_compile_options(consumer_demo PRIVATE -O2 -Wall -Wextra -Wpedantic)
target_compile_options(cleanup_demo PRIVATE -O2 -Wall -Wextra -Wpedantic)
