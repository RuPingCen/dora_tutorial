
该测试节点使用ROS2的fast_dds API接口，需要保证ROS2的DDS配置为 rmw_fastrtps_cpp
查看dds的类型命令  ros2 doctor --report | grep middleware
 
## 程序功能
topic_publisher 节点调用fast_dds API接口向/turtle1/cmd_vel发送数据，控制小海龟做圆周运动

## 运行DDS层数据获取节点

编译
cd topic_publisher
mkdir build
cd build
cmake ..
make

运行
./topic_publisher


## 启动小海龟
sudo apt install ros-humble-turtlesim

ros2 run turtlesim turtlesim_node

现在你可以看到小海龟在屏幕上画圆了