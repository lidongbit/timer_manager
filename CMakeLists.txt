cmake_minimum_required(VERSION 3.10)

project(MyTimer VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add the executable
add_executable(MyTimer main.cpp TimerThread.cpp)

target_include_directories(MyTimer PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )

find_package(Threads)
target_link_libraries(MyTimer ${CMAKE_THREAD_LIBS_INIT})
