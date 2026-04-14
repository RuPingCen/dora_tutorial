cmake_minimum_required(VERSION 3.10)
project(dora_cpp_nodes_tutorial)

# set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

set(CMAKE_CXX_FLAGS "-std=c++14 -pthread -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
# NOTE: CHOOSE THE O2/O3 is really important for speed! check more on optimization on compile
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# 参考 rslidar_driver 的路径设置
set(DORA_ROOT $ENV{HOME}/dora)
set(DORA_INCLUDE_DIR ${DORA_ROOT}/apis/c/node; ${DORA_ROOT}/apis/c/operator)
set(DORA_LIB_PATH ${DORA_ROOT}/target/release/libdora_node_api_c.a)

include_directories( 
    ${DORA_INCLUDE_DIR}
)

# Node A
add_executable(node_a node_a.cc)
target_link_libraries(node_a
    ${DORA_LIB_PATH}
    m
    rt
    dl
    pthread
)

# Node B
add_executable(node_b node_b.cc)
target_link_libraries(node_b
    ${DORA_LIB_PATH}
    m
    rt
    dl
    pthread
)
