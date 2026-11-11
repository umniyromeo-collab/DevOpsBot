#pragma once

#include "event_source.h"

#include <memory>
#include <string>

namespace RdKafka {
    class KafkaConsumer;
    class Message;
} // namespace RdKafka

namespace kafka {

    // Consumer топика site-status на librdkafka с ручным коммитом offset.
    // Используется одним потоком (поток доставки бота).
    class Consumer final : public IEventSource {
    public:
        Consumer(const std::string &brokers, const std::string &group_id);
        ~Consumer() override;

        Consumer(const Consumer &) = delete;
        Consumer &operator=(const Consumer &) = delete;

        std::optional<SiteStatusEvent> Next(std::chrono::milliseconds timeout) override;
        void Commit() override;

    private:
        std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
        // Сообщение, которое вернул последний Next: Commit фиксирует именно его offset.
        std::unique_ptr<RdKafka::Message> last_;
    };

} // namespace kafka
