cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(reactor)

set(CMAKE_DEBUG_POSTFIX d)

set(CXX_FLAGS
    -g
    # -DVALGRIND
    -DCHECK_PTHREAD_RETURN_VALUE
    -D_FILE_OFFSET_BITS=64
    -march=native
    # -MMD
    -rdynamic
    )
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

# 设置生成文件输出路径
set(EXECUTABLE_OUTPUT_PATH, ./output/debug/bin)
add_executable(reactor ./main/tcp_server_test.cc)

target_include_directories(reactor PUBLIC ./inc/)
target_include_directories(reactor PRIVATE ./extern_inc/)
target_include_directories(reactor PRIVATE ./reactor)

target_link_directories(reactor PRIVATE ./lib/debug/)

target_link_libraries(reactor PRIVATE utild)
target_link_libraries(reactor PRIVATE basicd)
target_link_libraries(reactor PRIVATE protocold)
target_link_libraries(reactor PRIVATE systemd)
target_link_libraries(reactor PRIVATE pthread)
target_link_libraries(reactor PRIVATE utild)
target_link_libraries(reactor PRIVATE algorithmd)
include(src/CMakeLists.txt)
include(reactor/CMakeLists.txt)

