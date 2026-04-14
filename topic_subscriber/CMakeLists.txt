cmake_minimum_required(VERSION 3.16)
project(topic_subscriber_node CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROS2_PREFIX /opt/ros/humble)

# Fast DDS 和 Fast CDR 头文件路径
include_directories(
    ${ROS2_PREFIX}/include/fastrtps   # fastdds/dds/... 和 fastrtps/...
    ${ROS2_PREFIX}/include/fastcdr    # fastcdr/Cdr.h 等
)

link_directories(${ROS2_PREFIX}/lib)

add_executable(topic_subscriber topic_subscriber.cpp)
target_link_libraries(topic_subscriber fastrtps fastcdr pthread)
