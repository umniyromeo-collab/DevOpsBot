#pragma once

#include "event_publisher.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace RdKafka {
    class Producer;
}

namespace kafka {

    // Producer топика site-status на librdkafka. Не потокобезопасен в нашем использовании:
    // Publish и отчёт о доставке обслуживаются в потоке, который вызывает Publish (поток записи).
    class Producer final : public IEventPublisher {
    public:
        // ack_timeout — сколько ждать подтверждения брокера, прежде чем считать отправку неудачной.
        Producer(const std::string &brokers, std::chrono::milliseconds ack_timeout);
        ~Producer() override;

        Producer(const Producer &) = delete;
        Producer &operator=(const Producer &) = delete;

        bool Publish(const SiteStatusEvent &event) override;

    private:
        class DeliveryReport;

        std::chrono::milliseconds ack_timeout_;
        std::uint64_t next_id_ = 0;
        // Объявлен до producer_: колбэк должен пережить producer, который его вызывает.
        std::unique_ptr<DeliveryReport> report_;
        std::unique_ptr<RdKafka::Producer> producer_;
    };

} // namespace kafka
