cmake_minimum_required(VERSION 3.10)

project(reactor VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_DEBUG_POSTFIX d)

# Compiler flags
set(CXX_FLAGS
    -g
    -ggdb
    -DCHECK_PTHREAD_RETURN_VALUE
    -D_FILE_OFFSET_BITS=64
    -march=native
    -rdynamic
    -O0 -Wall
    -DDEBUG
)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

# Output directories
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output/debug/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output/debug/lib)

# Source files
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cc
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.cxx
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/reactor/*.cc
    ${CMAKE_SOURCE_DIR}/reactor/*.cpp
    ${CMAKE_SOURCE_DIR}/reactor/*.cxx
    ${CMAKE_SOURCE_DIR}/reactor/*.c
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/inc)
include_directories(${CMAKE_SOURCE_DIR}/extern_inc)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/reactor)

# Link directories
link_directories(${CMAKE_SOURCE_DIR}/lib/debug)

# Build targets
add_library(reactor STATIC
    ${SRC_FILES}
)

add_executable(http_chunk_test
    ${CMAKE_SOURCE_DIR}/main/http_chunk_test.cc
    ${SRC_FILES}
)

add_executable(http_test
    ${CMAKE_SOURCE_DIR}/main/http_test.cc
    ${SRC_FILES}
)

add_executable(echo_server_test
    ${CMAKE_SOURCE_DIR}/main/echo_server_test.cc
    ${SRC_FILES}
)

add_executable(tcp_client_test
    ${CMAKE_SOURCE_DIR}/main/tcp_client_test.cc
    ${SRC_FILES}
)

add_executable(tcp_server_test
    ${CMAKE_SOURCE_DIR}/main/tcp_server_test.cc
    ${SRC_FILES}
)

add_executable(ws_send_test
    ${CMAKE_SOURCE_DIR}/main/ws_send_test.cc
    ${SRC_FILES}
)

# Include dirs for target reactor
target_include_directories(reactor PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(reactor PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(reactor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(reactor PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Include dirs for target http_chunk_test
target_include_directories(http_chunk_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(http_chunk_test PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(http_chunk_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(http_chunk_test PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Include dirs for target http_test
target_include_directories(http_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(http_test PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(http_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(http_test PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Include dirs for target echo_server_test
target_include_directories(echo_server_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(echo_server_test PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(echo_server_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(echo_server_test PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Include dirs for target tcp_client_test
target_include_directories(tcp_client_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(tcp_client_test PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(tcp_client_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(tcp_client_test PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Include dirs for target tcp_server_test
target_include_directories(tcp_server_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(tcp_server_test PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(tcp_server_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(tcp_server_test PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Include dirs for target ws_send_test
target_include_directories(ws_send_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(ws_send_test PRIVATE ${CMAKE_SOURCE_DIR}/extern_inc)
target_include_directories(ws_send_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(ws_send_test PRIVATE ${CMAKE_SOURCE_DIR}/reactor)

# Legacy dependencies
target_link_libraries(reactor
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)
target_link_libraries(http_chunk_test
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)
target_link_libraries(http_test
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)
target_link_libraries(echo_server_test
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)
target_link_libraries(tcp_client_test
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)
target_link_libraries(tcp_server_test
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)
target_link_libraries(ws_send_test
    util
    basic
    protocol
    algorithm
    system
    ncurses++
    form
    menu
    panel
    ncurses
    pthread
)

