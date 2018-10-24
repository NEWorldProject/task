cmake_minimum_required(VERSION 3.12)
project(task)

find_package(Boost REQUIRED COMPONENTS context)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC Library/*)

add_library(task SHARED ${SRC})
target_include_directories(task PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(task ${Boost_LIBRARIES})

add_executable(task_exp exp.cpp)
target_link_libraries(task_exp PRIVATE Boost::context task)
