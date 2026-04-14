# ROS2 原生 DDS 发布器 (dds_publisher)
该程序通过**原生 Fast DDS API** 实现 ROS2 兼容的话题发布功能，无需依赖 ROS2 的 rclcpp/rclpy 层，直接与 ROS2 生态的话题系统通信，可被 ROS2 原生工具（`ros2 topic echo`/`ros2 topic list`）识别和订阅。

## 目录
- [快速使用](#快速使用)
- [核心功能说明](#核心功能说明)
- [API 接口详解](#api接口详解)
- [注意事项](#注意事项)

## 快速使用
### 1. 编译依赖
确保系统已安装 Fast DDS 开发库（ROS2 安装时默认自带）：
```bash
# Ubuntu/Debian 环境（以 Humble 为例）
sudo apt install ros-humble-fastrtps ros-humble-fastcdr
```

### 2. 编译命令
使用 g++ 直接编译（需链接 Fast DDS 库）：
```bash
g++ dds_publisher.cpp -o dds_publisher -lfastrtps -lfastcdr -pthread
```

### 3. 运行程序
```bash
# 启动发布器
./dds_publisher

# 新开终端，用 ROS2 工具验证（可看到发布的消息）
ros2 topic echo /dds_chatter std_msgs/msg/String
```

### 4. 停止程序
按下 `Ctrl+C` 触发信号处理，程序会自动清理 DDS 资源并退出。

## 核心功能说明
| 模块/函数               | 功能描述                                                                 |
|-------------------------|--------------------------------------------------------------------------|
| `StringMsg` 结构体      | 匹配 ROS2 `std_msgs/msg/String` 消息结构，仅包含 `data` 字符串字段       |
| `StringMsgType` 类      | 自定义 DDS 数据类型支持类，实现 ROS2 消息的序列化/反序列化、类型注册     |
| `main()` 主流程         | 完整的 DDS 发布器生命周期：创建参与者 → 注册类型 → 创建话题 → 发布消息 → 资源清理 |
| `sig_handler()` 信号处理 | 捕获 `Ctrl+C` 信号，安全终止循环并触发资源释放                           |

### 关键逻辑说明
1. **Domain 匹配**：使用 DDS Domain 0（ROS2 默认 Domain），确保与 ROS2 节点在同一通信域；
2. **话题命名规范**：DDS 话题名 `rt/dds_chatter` 对应 ROS2 话题名 `/dds_chatter`（ROS2 会自动去掉 `rt/` 前缀）；
3. **QoS 配置**：DataWriter QoS 与 ROS2 默认一致（可靠传输、易失性持久化、保留最后10条消息）；
4. **资源管理**：程序退出时主动删除 DataWriter/Publisher/Topic/Participant，避免内存泄漏。

## API 接口详解
### 1. Fast DDS 核心类（关键接口）
| 类名                  | 核心方法                          | 作用                                                                 |
|-----------------------|-----------------------------------|----------------------------------------------------------------------|
| `DomainParticipant`   | `create_participant()`            | 创建 DDS 域参与者（通信入口），参数：Domain ID、QoS 配置             |
|                       | `create_topic()`                  | 创建话题，参数：话题名、数据类型名、Topic QoS                        |
|                       | `create_publisher()`              | 创建发布器，参数：Publisher QoS                                      |
| `TypeSupport`         | `register_type()`                 | 向参与者注册自定义数据类型（如 `StringMsgType`）                     |
| `DataWriter`          | `write()`                         | 发布消息，参数：待发布的消息结构体指针                               |
| `TopicDataType`       | `serialize()`/`deserialize()`     | 实现消息与 DDS 二进制载荷的互转（兼容 ROS2 CDR 编码格式）             |
|                       | `createData()`/`deleteData()`     | 创建/销毁消息实例，供 DDS 底层调用                                   |

### 2. 自定义类型类 `StringMsgType` 关键方法
| 方法名                | 功能                                                                 |
|-----------------------|----------------------------------------------------------------------|
| `setName()`           | 设置类型名（必须为 `std_msgs::msg::dds_::String_`，ROS2 固定命名规则） |
| `serialize()`         | 将 `StringMsg` 序列化为 DDS 二进制载荷（带 CDR 封装头）               |
| `deserialize()`       | 将 DDS 二进制载荷反序列化为 `StringMsg`                               |
| `getSerializedSizeProvider()` | 计算消息序列化后的长度（用于 DDS 底层内存分配）               |

### 3. QoS 配置（与 ROS2 对齐）
程序中 DataWriter 的 QoS 配置对应 ROS2 默认值：
```cpp
wqos.reliability().kind  = RELIABLE_RELIABILITY_QOS;  // 可靠传输（确保消息送达）
wqos.durability().kind   = VOLATILE_DURABILITY_QOS;   // 易失性（不持久化消息）
wqos.history().kind      = KEEP_LAST_HISTORY_QOS;     // 保留最后 N 条消息
wqos.history().depth     = 10;                        // 保留最近 10 条
```

## 注意事项
1. **类型名必须严格匹配**：ROS2 对 DDS 类型名有固定格式（`包名::msg::dds_::消息名_`），修改会导致 ROS2 无法识别；
2. **Domain ID 一致性**：ROS2 默认使用 Domain 0，若需修改需确保所有通信方 Domain ID 一致；
3. **资源清理**：必须显式调用 `delete_xxx()` 方法清理 DDS 资源（否则会导致进程残留）；
4. **消息兼容性**：该示例仅实现 `std_msgs/msg/String`，扩展其他消息需同步修改 `StringMsg` 和 `StringMsgType` 的序列化逻辑。

---

## 总结
1. 该程序核心是**绕过 ROS2 中间层**，通过原生 Fast DDS API 实现与 ROS2 话题的兼容通信；
2. 关键是匹配 ROS2 的**消息类型名**、**话题命名规则**和**QoS 配置**，确保与 ROS2 工具链互通；
3. 核心类 `StringMsgType` 实现了 ROS2 消息的 CDR 序列化/反序列化，是兼容 ROS2 的核心。
