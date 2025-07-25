#include "kafka_producer.hpp"

#include <iostream>

#include "rdkafka.h"
#include "utils/logger.hpp"

KafkaProducer::KafkaProducer(const std::string& brokers,
                             const std::string& topic)
    : topic_(topic), rk_(nullptr), conf_(nullptr) {
  char errstr[512];
  conf_ = rd_kafka_conf_new();
  if (rd_kafka_conf_set(conf_, "bootstrap.servers", brokers.c_str(), errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    LOG(ERROR) << "Failed to set Kafka brokers: " << errstr;
    throw std::runtime_error("Failed to configure Kafka: " +
                             std::string(errstr));
  }
  rk_ = rd_kafka_new(RD_KAFKA_PRODUCER, conf_, errstr, sizeof(errstr));
  if (!rk_) {
    LOG(ERROR) << "Failed to create Kafka producer: " << errstr;
    throw std::runtime_error("Failed to create Kafka producer: " +
                             std::string(errstr));
  }
  LOG(INFO) << "KafkaProducer initialized, brokers: " << brokers
            << ", topic: " << topic;
}

KafkaProducer::~KafkaProducer() {
  if (rk_) {
    rd_kafka_flush(rk_, 1000);
    rd_kafka_destroy(rk_);
    LOG(INFO) << "KafkaProducer destroyed";
  }
}

bool KafkaProducer::send(const std::string& message) {
  if (!rk_) {
    LOG(ERROR) << "KafkaProducer not initialized";
    return false;
  }
  int err = rd_kafka_producev(
      rk_, RD_KAFKA_V_TOPIC(topic_.c_str()),
      RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
      RD_KAFKA_V_VALUE((void*)message.c_str(), message.size()), RD_KAFKA_V_END);

  if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    LOG(ERROR) << "Failed to produce message: "
               << rd_kafka_err2str(static_cast<rd_kafka_resp_err_t>(err));
    return false;
  }

  LOG(DEBUG) << "Kafka message produced: " << message;
  return true;
}