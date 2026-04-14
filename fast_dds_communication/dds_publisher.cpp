/**
 * dds_publisher.cpp
 * 直接使用 Fast DDS API 发布消息，兼容 ROS2 topic 命名规范
 * 可通过 ros2 topic list / ros2 topic echo /dds_chatter 看到消息
 */

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
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
// ROS2 std_msgs/msg/String 消息结构
// -------------------------------------------------------
struct StringMsg {
    std::string data;
};

// -------------------------------------------------------
// 自定义 TypeSupport，匹配 ROS2 的 std_msgs/msg/String
// 类型名必须为 "std_msgs::msg::dds_::String_"
// -------------------------------------------------------
class StringMsgType : public TopicDataType {
public:
    StringMsgType() {
        setName("std_msgs::msg::dds_::String_");
        m_typeSize = 4096;          // 最大序列化大小（含 CDR 头）
        m_isGetKeyDefined = false;
        auto_fill_type_object(false);
        auto_fill_type_information(false);
    }

    bool serialize(void* data, SerializedPayload_t* payload) override {
        auto* msg = static_cast<StringMsg*>(data);
        eprosima::fastcdr::FastBuffer buf(
            reinterpret_cast<char*>(payload->data), payload->max_size);
        eprosima::fastcdr::Cdr ser(buf,
            eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
            eprosima::fastcdr::Cdr::DDS_CDR);
        ser.serialize_encapsulation();
        ser << msg->data;
        payload->length = static_cast<uint32_t>(ser.getSerializedDataLength());
        return true;
    }

    bool deserialize(SerializedPayload_t* payload, void* data) override {
        auto* msg = static_cast<StringMsg*>(data);
        eprosima::fastcdr::FastBuffer buf(
            reinterpret_cast<char*>(payload->data), payload->length);
        eprosima::fastcdr::Cdr deser(buf,
            eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
            eprosima::fastcdr::Cdr::DDS_CDR);
        deser.read_encapsulation();
        deser >> msg->data;
        return true;
    }

    std::function<uint32_t()> getSerializedSizeProvider(void* data) override {
        return [data]() -> uint32_t {
            auto* msg = static_cast<StringMsg*>(data);
            return static_cast<uint32_t>(4 + 4 + msg->data.size() + 1);
        };
    }

    void* createData() override { return new StringMsg(); }
    void deleteData(void* data) override { delete static_cast<StringMsg*>(data); }

    bool getKey(void* /*data*/, InstanceHandle_t* /*handle*/, bool /*force_md5*/) override {
        return false;
    }
};

// -------------------------------------------------------
// 主程序
// -------------------------------------------------------
static volatile bool running = true;
void sig_handler(int) { running = false; }

int main()
{
    std::signal(SIGINT, sig_handler);

    // 1. 创建 DomainParticipant（Domain 0，与 ROS2 默认一致）
    DomainParticipantQos pqos;
    pqos.name("dds_test_publisher");

    DomainParticipant* participant =
        DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (!participant) { std::cerr << "create_participant failed\n"; return 1; }

    // 2. 注册类型
    TypeSupport type_support(new StringMsgType());
    type_support.register_type(participant);

    // 3. 创建 Topic
    //    ROS2 规范：DDS topic 名 = "rt/" + ROS2 topic 名
    //    => ros2 topic list 会显示 /dds_chatter
    Topic* topic = participant->create_topic(
        "rt/dds_chatter",
        "std_msgs::msg::dds_::String_",
        TOPIC_QOS_DEFAULT);
    if (!topic) { std::cerr << "create_topic failed\n"; return 1; }

    // 4. 创建 Publisher
    Publisher* publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
    if (!publisher) { std::cerr << "create_publisher failed\n"; return 1; }

    // 5. 创建 DataWriter（QoS 与 ROS2 默认保持一致）
    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    wqos.reliability().kind  = RELIABLE_RELIABILITY_QOS;
    wqos.durability().kind   = VOLATILE_DURABILITY_QOS;
    wqos.history().kind      = KEEP_LAST_HISTORY_QOS;
    wqos.history().depth     = 10;

    DataWriter* writer = publisher->create_datawriter(topic, wqos);
    if (!writer) { std::cerr << "create_datawriter failed\n"; return 1; }

    std::cout << "=== DDS Publisher started ===" << std::endl
              << "ros2 topic list     -> /dds_chatter" << std::endl
              << "ros2 topic echo /dds_chatter std_msgs/msg/String" << std::endl
              << "Ctrl+C to stop" << std::endl;

    StringMsg msg;
    int count = 0;
    while (running) {
        msg.data = "Hello from raw DDS! count=" + std::to_string(count++);
        writer->write(&msg);
        std::cout << "[pub] " << msg.data << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 清理
    publisher->delete_datawriter(writer);
    participant->delete_publisher(publisher);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);
    std::cout << "Publisher stopped.\n";
    return 0;
}
