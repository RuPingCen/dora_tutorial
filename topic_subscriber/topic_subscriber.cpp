/**
 * turtlebot_subscriber.cpp
 * 直接使用 Fast DDS API 订阅 ROS2 TurtleBot3 的键盘控制命令 (/cmd_vel)
 * 消息类型：geometry_msgs/msg/Twist
 */

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
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
#include <iomanip>

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

// -------------------------------------------------------
// ROS2 geometry_msgs/msg/Twist 消息结构
// -------------------------------------------------------
struct Vector3 {
    double x, y, z;
};

struct Twist {
    Vector3 linear;
    Vector3 angular;
};

// -------------------------------------------------------
// 自定义 TypeSupport，匹配 ROS2 的 geometry_msgs/msg/Twist
// 类型名必须为 "geometry_msgs::msg::dds_::Twist_"
// -------------------------------------------------------
class TwistPubSubType : public TopicDataType {
public:
    TwistPubSubType() {
        setName("geometry_msgs::msg::dds_::Twist_");
        // 修正大小：4字节头 + 4字节对齐填充 + 48字节数据 = 56字节
        m_typeSize = 56;
        m_isGetKeyDefined = false;
        auto_fill_type_object(false);
        auto_fill_type_information(false);
    }

    bool serialize(void* data, SerializedPayload_t* payload) override {
        Twist* twist = static_cast<Twist*>(data);
        eprosima::fastcdr::FastBuffer buf(reinterpret_cast<char*>(payload->data), payload->max_size);
        eprosima::fastcdr::Cdr ser(buf, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
        ser.serialize_encapsulation();
        ser << twist->linear.x << twist->linear.y << twist->linear.z;
        ser << twist->angular.x << twist->angular.y << twist->angular.z;
        payload->length = static_cast<uint32_t>(ser.getSerializedDataLength());
        return true;
    }

    bool deserialize(SerializedPayload_t* payload, void* data) override {
        Twist* twist = static_cast<Twist*>(data);
        eprosima::fastcdr::FastBuffer buf(reinterpret_cast<char*>(payload->data), payload->length);
        eprosima::fastcdr::Cdr deser(buf, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
        deser.read_encapsulation();
        try {
            deser >> twist->linear.x >> twist->linear.y >> twist->linear.z;
            deser >> twist->angular.x >> twist->angular.y >> twist->angular.z;
        } catch (eprosima::fastcdr::exception::Exception& ex) {
            return false;
        }
        return true;
    }

    std::function<uint32_t()> getSerializedSizeProvider(void*) override {
        return []() -> uint32_t { return 56; };
    }

    void* createData() override { return new Twist(); }
    void deleteData(void* data) override { delete static_cast<Twist*>(data); }
    bool getKey(void*, InstanceHandle_t*, bool) override { return false; }
};

// -------------------------------------------------------
// DataReader 回调监听器
// -------------------------------------------------------
class TwistListener : public DataReaderListener {
public:
    void on_data_available(DataReader* reader) override {
        Twist twist;
        SampleInfo info;
        while (reader->take_next_sample(&twist, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.valid_data) {
                // 增加详细打印，包括纳秒级时间戳，以便区分这“三条消息”是否是同一次触发产生的
                long long ts = info.source_timestamp.to_ns();
                std::cout << "[Msg Received] TS: " << ts << "\n"
                          << "  Linear:  x=" << std::setw(5) << twist.linear.x 
                          << " y=" << std::setw(5) << twist.linear.y 
                          << " z=" << std::setw(5) << twist.linear.z << "\n"
                          << "  Angular: x=" << std::setw(5) << twist.angular.x 
                          << " y=" << std::setw(5) << twist.angular.y 
                          << " z=" << std::setw(5) << twist.angular.z << "\n"
                          << "-------------------------------------------" << std::endl;
            }
        }
    }

    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& s) override {
        if (s.current_count_change == 1) {
            std::cout << "\n[Status] Publisher matched! (Total: " << s.current_count << ")" << std::endl;
        } else if (s.current_count_change == -1) {
            std::cout << "\n[Status] Publisher unmatched. (Total: " << s.current_count << ")" << std::endl;
        }
    }
};

static volatile bool running = true;
void sig_handler(int) { running = false; }

int main()
{
    std::signal(SIGINT, sig_handler);

    // 1. 创建 DomainParticipant (Domain 0)
    DomainParticipantQos pqos;
    pqos.name("turtlebot_teleop_subscriber");
    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (!participant) return 1;

    // 2. 注册类型
    TypeSupport type(new TwistPubSubType());
    type.register_type(participant);

    // 3. 创建 Topic (ROS2 topic /cmd_vel -> DDS topic rt/cmd_vel)
    Topic* topic = participant->create_topic("rt/cmd_vel", "geometry_msgs::msg::dds_::Twist_", TOPIC_QOS_DEFAULT);
    if (!topic) return 1;

    // 4. 创建 Subscriber
    Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    if (!subscriber) return 1;

    // 5. 创建 DataReader
    // 为了最大限度兼容不同的 ROS2 节点，我们将 Reliability 设置为 BEST_EFFORT
    // BEST_EFFORT 的 Reader 可以匹配 RELIABLE 或 BEST_EFFORT 的 Writer
    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS; 
    rqos.durability().kind = VOLATILE_DURABILITY_QOS;
    rqos.history().kind = KEEP_LAST_HISTORY_QOS;
    rqos.history().depth = 10;

    TwistListener listener;
    DataReader* reader = subscriber->create_datareader(topic, rqos, &listener);
    if (!reader) return 1;

    std::cout << "=== TurtleBot3 Twist Subscriber Started ===" << std::endl;
    std::cout << "Listening on ROS2 topic: /cmd_vel" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 清理资源
    subscriber->delete_datareader(reader);
    participant->delete_subscriber(subscriber);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);
    std::cout << "\nSubscriber stopped." << std::endl;

    return 0;
}
