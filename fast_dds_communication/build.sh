#!/bin/bash
set -e
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo ""
echo "Build done. Binaries: build/dds_publisher  build/dds_subscriber"
echo ""
echo "Usage:"
echo "  Terminal 1: ./build/dds_publisher"
echo "  Terminal 2: ros2 topic list"
echo "  Terminal 2: ros2 topic echo /dds_chatter std_msgs/msg/String"
echo "  Terminal 3: ./build/dds_subscriber"
