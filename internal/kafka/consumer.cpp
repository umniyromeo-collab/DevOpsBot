#include "consumer.h"

#include <rdkafkacpp.h>

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace kafka {

    namespace {

        void Set(RdKafka::Conf &conf, const std::string &name, const std::string &value) {
            std::string error;
            if (conf.set(name, value, error) != RdKafka::Conf::CONF_OK) {
                throw std::runtime_error("Kafka consumer: " + name + ": " + error);
            }
        }

    } // namespace

    Consumer::Consumer(const std::string &brokers, const std::string &group_id) {
        std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        Set(*conf, "bootstrap.servers", brokers);
        Set(*conf, "group.id", group_id);
        // Offset коммитим сами и только после отправки всем подписчикам: упал до коммита —
        // после перезапуска событие придёт снова.
        Set(*conf, "enable.auto.commit", "false");
        // Первый запуск бота читает топик с начала: события, пока бота не было, не теряются.
        Set(*conf, "auto.offset.reset", "earliest");
        // Бот может стартовать раньше первого события, когда топика ещё нет. Без этого
        // librdkafka узнала бы о новом топике только при плановом обновлении метаданных (до 5 мин).
        Set(*conf, "allow.auto.create.topics", "true");

        std::string error;
        consumer_.reset(RdKafka::KafkaConsumer::create(conf.get(), error));
        if (!consumer_) {
            throw std::runtime_error("Kafka consumer: " + error);
        }
        const RdKafka::ErrorCode code = consumer_->subscribe({SITE_STATUS_TOPIC});
        if (code != RdKafka::ERR_NO_ERROR) {
            throw std::runtime_error("Kafka consumer: subscribe: " + RdKafka::err2str(code));
        }
    }

    Consumer::~Consumer() {
        last_.reset();
        consumer_->close();
    }

    std::optional<SiteStatusEvent> Consumer::Next(std::chrono::milliseconds timeout) {
        std::unique_ptr<RdKafka::Message> message(
            consumer_->consume(static_cast<int>(timeout.count())));

        switch (message->err()) {
        case RdKafka::ERR__TIMED_OUT:
            return std::nullopt;

        case RdKafka::ERR_NO_ERROR: {
            const auto *data = static_cast<const char *>(message->payload());
            const std::string_view payload =
                data != nullptr ? std::string_view{data, message->len()} : std::string_view{};
            auto event = FromJson(payload);
            if (!event) {
                // Битое сообщение не разобрать никогда: пропускаем и коммитим, иначе бот
                // застрял бы на нём навсегда.
                std::cerr << "Kafka: не удалось разобрать событие (offset " << message->offset()
                          << "), пропускаем" << std::endl;
                consumer_->commitSync(message.get());
                return std::nullopt;
            }
            last_ = std::move(message);
            return event;
        }

        default:
            // Например, топик ещё не создан или брокер недоступен: librdkafka переподключится
            // сама, нам достаточно попробовать снова.
            std::cerr << "Kafka: " << message->errstr() << std::endl;
            return std::nullopt;
        }
    }

    void Consumer::Commit() {
        if (!last_) {
            return;
        }
        const RdKafka::ErrorCode code = consumer_->commitSync(last_.get());
        if (code != RdKafka::ERR_NO_ERROR) {
            // Сообщения уже отправлены; без коммита событие придёт ещё раз — дубль, не потеря.
            std::cerr << "Kafka: не удалось закоммитить offset: " << RdKafka::err2str(code)
                      << std::endl;
        }
        last_.reset();
    }

} // namespace kafka
