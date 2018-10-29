cmake_minimum_required(VERSION 3.12)
project(task)
set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS context)

file(GLOB_RECURSE SRC Library/*)
add_library(task SHARED ${SRC})
target_link_libraries(task PRIVATE Boost::context)
#System Check
if (WIN32)
    target_compile_definitions(task PUBLIC -DTASK_TARGET_WINDOWS)
elseif (APPLE)
    target_compile_definitions(task PUBLIC -DTASK_TARGET_APPLE)
elseif (UNIX)
    # We Assume Unix Systems Use X11 Desktop except APPL
    target_compile_definitions(task PUBLIC -DTASK_TARGET_UNIX_X11)
endif ()
# Enable IPO
include(CheckIPOSupported)
check_ipo_supported(RESULT HAS_IPO OUTPUT MESSAGE)
if(HAS_IPO)
    set_property(TARGET task PROPERTY INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Debug>:FALSE>:TRUE)
else()
    message(WARNING "IPO is not supported: ${MESSAGE}")
endif()

add_executable(task_exp exp.cpp)
target_link_libraries(task_exp PRIVATE task)


