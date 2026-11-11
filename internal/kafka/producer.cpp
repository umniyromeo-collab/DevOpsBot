#include "producer.h"

#include <rdkafkacpp.h>

#include <iostream>
#include <stdexcept>

namespace kafka {

    namespace {

        void Set(RdKafka::Conf &conf, const std::string &name, const std::string &value) {
            std::string error;
            if (conf.set(name, value, error) != RdKafka::Conf::CONF_OK) {
                throw std::runtime_error("Kafka producer: " + name + ": " + error);
            }
        }

    } // namespace

    // Отчёт о доставке. librdkafka вызывает его из poll()/flush(), то есть в потоке, который
    // ждёт подтверждения в Publish, — синхронизация не нужна.
    class Producer::DeliveryReport final : public RdKafka::DeliveryReportCb {
    public:
        void dr_cb(RdKafka::Message &message) override {
            // Отчёт по сообщению, которое Publish уже перестал ждать (истёк свой таймаут),
            // не должен засчитаться следующему сообщению — сравниваем номер.
            if (reinterpret_cast<std::uintptr_t>(message.msg_opaque()) != awaited) {
                return;
            }
            done = true;
            ok = message.err() == RdKafka::ERR_NO_ERROR;
            error = ok ? std::string{} : message.errstr();
        }

        std::uintptr_t awaited = 0;
        bool done = false;
        bool ok = false;
        std::string error;
    };

    Producer::Producer(const std::string &brokers, std::chrono::milliseconds ack_timeout)
        : ack_timeout_(ack_timeout), report_(std::make_unique<DeliveryReport>()) {
        std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        Set(*conf, "bootstrap.servers", brokers);
        // Подтверждение от всех реплик и идемпотентность: повтор внутри librdkafka не даёт
        // дубликатов в топике, а подтверждённое событие не потеряется на брокере.
        Set(*conf, "acks", "all");
        Set(*conf, "enable.idempotence", "true");
        // librdkafka сама сообщит о неудаче не позже этого срока — Publish не повиснет навсегда.
        Set(*conf, "message.timeout.ms", std::to_string(ack_timeout_.count()));

        std::string error;
        if (conf->set("dr_cb", report_.get(), error) != RdKafka::Conf::CONF_OK) {
            throw std::runtime_error("Kafka producer: dr_cb: " + error);
        }
        producer_.reset(RdKafka::Producer::create(conf.get(), error));
        if (!producer_) {
            throw std::runtime_error("Kafka producer: " + error);
        }
    }

    Producer::~Producer() {
        // Всё, что Publish отправил, он уже дождался; flush — на случай отчётов, которые
        // пришли после таймаута ожидания.
        producer_->flush(static_cast<int>(ack_timeout_.count()));
    }

    bool Producer::Publish(const SiteStatusEvent &event) {
        const std::string payload = ToJson(event);
        const auto id = static_cast<std::uintptr_t>(++next_id_);
        report_->awaited = id;
        report_->done = false;
        report_->ok = false;

        const RdKafka::ErrorCode code = producer_->produce(
            SITE_STATUS_TOPIC, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
            const_cast<char *>(payload.data()), payload.size(), event.url.data(), event.url.size(),
            0, reinterpret_cast<void *>(id));
        if (code != RdKafka::ERR_NO_ERROR) {
            std::cerr << "Kafka: не удалось отправить событие по " << event.url << ": "
                      << RdKafka::err2str(code) << std::endl;
            return false;
        }

        // Отчёт придёт не позже message.timeout.ms; секунда сверху — запас на сам poll.
        const auto deadline =
            std::chrono::steady_clock::now() + ack_timeout_ + std::chrono::seconds{1};
        while (!report_->done && std::chrono::steady_clock::now() < deadline) {
            producer_->poll(100);
        }
        if (!report_->done) {
            std::cerr << "Kafka: нет подтверждения события по " << event.url << std::endl;
            return false;
        }
        if (!report_->ok) {
            std::cerr << "Kafka: брокер не принял событие по " << event.url << ": "
                      << report_->error << std::endl;
        }
        return report_->ok;
    }

} // namespace kafka
