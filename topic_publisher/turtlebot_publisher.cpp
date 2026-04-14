/**
 * turtlebot_publisher.cpp
 * 直接使用 Fast DDS API 向 ROS2 /cmd_vel 话题发布控制指令
 * 消息类型：geometry_msgs/msg/Twist
 * 功能：控制机器人（或小海龟）走圆圈
 */

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/rtps/common/SerializedPayload.h>
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

// -------------------------------------------------------
// ROS2 geometry_msgs/msg/Twist 消息结构
// -------------------------------------------------------
struct Vector3
{
    double x = 0, y = 0, z = 0;
};

struct Twist
{
    Vector3 linear;
    Vector3 angular;
};

// -------------------------------------------------------
// 自定义 TypeSupport，匹配 ROS2 的 geometry_msgs/msg/Twist
// 必须处理 4字节头 + 4字节对齐填充
// -------------------------------------------------------
class TwistPubSubType : public TopicDataType
{
public:
    TwistPubSubType()
    {
        setName("geometry_msgs::msg::dds_::Twist_");
        m_typeSize = 56; // 4(header) + 4(padding) + 24(linear) + 24(angular)
        m_isGetKeyDefined = false;
        auto_fill_type_object(false);
        auto_fill_type_information(false);
    }

    bool serialize(void *data, SerializedPayload_t *payload) override
    {
        Twist *twist = static_cast<Twist *>(data);
        eprosima::fastcdr::FastBuffer buf(reinterpret_cast<char *>(payload->data), payload->max_size);
        eprosima::fastcdr::Cdr ser(buf, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
        ser.serialize_encapsulation();
        // 第一个 double 写入前，FastCDR 会自动根据当前偏移(4)插入 4字节对齐填充
        ser << twist->linear.x << twist->linear.y << twist->linear.z;
        ser << twist->angular.x << twist->angular.y << twist->angular.z;
        payload->length = static_cast<uint32_t>(ser.getSerializedDataLength());
        return true;
    }

    bool deserialize(SerializedPayload_t *payload, void *data) override
    {
        Twist *twist = static_cast<Twist *>(data);
        eprosima::fastcdr::FastBuffer buf(reinterpret_cast<char *>(payload->data), payload->length);
        eprosima::fastcdr::Cdr deser(buf, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
        deser.read_encapsulation();
        deser >> twist->linear.x >> twist->linear.y >> twist->linear.z;
        deser >> twist->angular.x >> twist->angular.y >> twist->angular.z;
        return true;
    }

    std::function<uint32_t()> getSerializedSizeProvider(void *) override
    {
        return []() -> uint32_t
        { return 56; };
    }

    void *createData() override { return new Twist(); }
    void deleteData(void *data) override { delete static_cast<Twist *>(data); }
    bool getKey(void *, InstanceHandle_t *, bool) override { return false; }
};

static volatile bool running = true;
void sig_handler(int) { running = false; }

int main()
{
    std::signal(SIGINT, sig_handler);

    // 1. 创建 DomainParticipant (Domain 0)
    DomainParticipantQos pqos;
    pqos.name("turtle_circle_publisher");
    DomainParticipant *participant = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (!participant)
        return 1;

    // 2. 注册类型
    TypeSupport type(new TwistPubSubType());
    type.register_type(participant);

    // 3. 创建 Topic (rt/cmd_vel)
    Topic *topic = participant->create_topic("rt/turtle1/cmd_vel", "geometry_msgs::msg::dds_::Twist_", TOPIC_QOS_DEFAULT);
    if (!topic)
        return 1;

    // 4. 创建 Publisher
    Publisher *publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
    if (!publisher)
        return 1;

    // 5. 创建 DataWriter
    // 与 ROS2 默认兼容的 QoS: Reliability: Reliable, Durability: Volatile
    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    wqos.durability().kind = VOLATILE_DURABILITY_QOS;

    DataWriter *writer = publisher->create_datawriter(topic, wqos);
    if (!writer)
        return 1;

    std::cout << "=== FastDDS Turtle Circle Publisher Started ===" << std::endl;
    std::cout << "Publishing to: /cmd_vel" << std::endl;

    Twist cmd;
    // 线速度 2.0, 角速度 1.0 -> 半径为 2.0 的圆
    cmd.linear.x = 2.0;
    cmd.angular.z = 1.0;

    while (running)
    {
        writer->write(&cmd);
        std::cout << "[Pub] Linear.x: " << cmd.linear.x << ", Angular.z: " << cmd.angular.z << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz
    }

    // 退出前发送停止指令
    cmd.linear.x = 0;
    cmd.angular.z = 0;
    writer->write(&cmd);
    std::cout << "Sent stop command." << std::endl;

    // 清理资源
    publisher->delete_datawriter(writer);
    participant->delete_publisher(publisher);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return 0;
}
