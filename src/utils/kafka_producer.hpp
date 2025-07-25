#pragma once
#include <string>
struct rd_kafka_s;
struct rd_kafka_conf_s;

class KafkaProducer {
public:
    KafkaProducer(const std::string& brokers, const std::string& topic);
    ~KafkaProducer();
    bool send(const std::string& message);

private:
    std::string topic_;
    rd_kafka_s* rk_;
    rd_kafka_conf_s* conf_;
};