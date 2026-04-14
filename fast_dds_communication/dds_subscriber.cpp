/**
 * dds_subscriber.cpp
 * 直接使用 Fast DDS API 订阅 ROS2 发布的 std_msgs/msg/String 消息
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

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

struct StringMsg { std::string data; };

// 与 publisher 相同的 TypeSupport
class StringMsgType : public TopicDataType {
public:
    StringMsgType() {
        setName("std_msgs::msg::dds_::String_");
        m_typeSize = 4096;
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
    bool getKey(void*, InstanceHandle_t*, bool) override { return false; }
};

// -------------------------------------------------------
// DataReader 回调监听器
// -------------------------------------------------------
class StringListener : public DataReaderListener {
public:
    void on_data_available(DataReader* reader) override {
        StringMsg msg;
        SampleInfo info;
        if (reader->take_next_sample(&msg, &info) == ReturnCode_t::RETCODE_OK) {
            if (info.valid_data) {
                std::cout << "[sub] received: " << msg.data << "\n";
            }
        }
    }

    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& s) override {
        std::cout << "[sub] matched publishers: " << s.current_count << "\n";
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

    DomainParticipantQos pqos;
    pqos.name("dds_test_subscriber");

    DomainParticipant* participant =
        DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (!participant) { std::cerr << "create_participant failed\n"; return 1; }

    TypeSupport type_support(new StringMsgType());
    type_support.register_type(participant);

    Topic* topic = participant->create_topic(
        "rt/dds_chatter",
        "std_msgs::msg::dds_::String_",
        TOPIC_QOS_DEFAULT);
    if (!topic) { std::cerr << "create_topic failed\n"; return 1; }

    Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    if (!subscriber) { std::cerr << "create_subscriber failed\n"; return 1; }

    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rqos.durability().kind  = VOLATILE_DURABILITY_QOS;
    rqos.history().kind     = KEEP_LAST_HISTORY_QOS;
    rqos.history().depth    = 10;

    StringListener listener;
    DataReader* reader = subscriber->create_datareader(topic, rqos, &listener);
    if (!reader) { std::cerr << "create_datareader failed\n"; return 1; }

    std::cout << "=== DDS Subscriber started, waiting on /dds_chatter ===\n"
              << "Ctrl+C to stop\n\n";

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    subscriber->delete_datareader(reader);
    participant->delete_subscriber(subscriber);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);
    std::cout << "Subscriber stopped.\n";
    return 0;
}
