cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(reactor)

set(CMAKE_DEBUG_POSTFIX d)

# 指定c++标准: 默认是 c++11
add_library(reactor_compiler_flags INTERFACE)
target_compile_features(reactor_compiler_flags INTERFACE cxx_std_11)
# 设置 c++ 警告标志
set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

target_compile_options(reactor_compiler_flags INTERFACE
		$<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wall;-Wextra;-Wshadow;-Wformat=2;-Wunused>>
		$<${msvc_cxx}:$<BUILD_INTERFACE:-W3>>)

# 设置生成文件输出路径
set(EXECUTABLE_OUTPUT_PATH, ./output/debug/bin)
add_executable(reactor ./main/client_test.cc)

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

