#pragma once

// Фейковый «брокер» для юнит-тестов потока записи: события копятся в векторе.
// Пишет вызовы в тот же журнал, что и FakeCheckerStore, и умеет «падать» до и после отправки.

#include "../../kafka/event_publisher.h"
#include "fake_checker_store.h"

#include <string>
#include <vector>

namespace monitor::testing {

    class FakePublisher final : public kafka::IEventPublisher {
    public:
        explicit FakePublisher(std::vector<std::string> *journal = nullptr) : journal_(journal) {}

        // Что принял «брокер».
        std::vector<kafka::SiteStatusEvent> events;

        // false — брокер не подтвердил приём (недоступен, таймаут), событие не принято.
        bool ack = true;
        // Упасть до отправки: событие не дошло до брокера (один раз).
        bool crash_before_send = false;
        // Упасть после того, как брокер принял событие, но до ответа (один раз).
        bool crash_after_send = false;

        bool Publish(const kafka::SiteStatusEvent &event) override {
            if (crash_before_send) {
                crash_before_send = false;
                throw SimulatedCrash{};
            }
            if (journal_ != nullptr) {
                journal_->push_back("publish " + event.url + (event.is_up ? " up" : " down"));
            }
            if (!ack) {
                return false;
            }
            events.push_back(event);
            if (crash_after_send) {
                crash_after_send = false;
                throw SimulatedCrash{};
            }
            return true;
        }

    private:
        std::vector<std::string> *journal_;
    };

} // namespace monitor::testing
