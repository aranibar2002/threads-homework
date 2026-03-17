cmake_minimum_required(VERSION 3.16)

project(thread_pool_future LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(thread_pool_compile_check compile_check.cpp)
target_link_libraries(thread_pool_compile_check PRIVATE Threads::Threads)
