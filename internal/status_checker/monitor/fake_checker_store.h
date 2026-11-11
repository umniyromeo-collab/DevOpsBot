#pragma once

// Фейковое хранилище чекера для юнит-тестов: всё в памяти, БД не нужна.
// Пишет каждый вызов в общий журнал, чтобы тесты проверяли порядок «запись → Kafka → отметка»,
// и умеет «падать» на выбранном шаге, имитируя падение процесса.

#include "../../storage/ichecker_store.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace monitor::testing {

    // Исключение, которым фейки изображают падение процесса посреди операции.
    struct SimulatedCrash : std::runtime_error {
        SimulatedCrash() : std::runtime_error("simulated crash") {}
    };

    class FakeCheckerStore final : public storage::ICheckerStore {
    public:
        explicit FakeCheckerStore(std::vector<std::string> *journal = nullptr)
            : journal_(journal) {}

        // Что лежит «в БД».
        std::vector<storage::Site> sites;
        std::map<std::string, SiteStatus> statuses;
        std::vector<CheckerResult> results;
        std::map<std::string, int> recent_failures;

        // Упасть на ближайшем вызове этого шага (один раз).
        bool crash_on_save = false;
        bool crash_on_mark = false;
        int recent_failures_calls = 0;

        std::vector<storage::Site> LoadSites() override {
            return sites;
        }

        std::map<std::string, SiteStatus> LoadStatuses() override {
            for (const auto &site : sites) {
                statuses.try_emplace(site.url); // как INSERT ... ON CONFLICT DO NOTHING
            }
            return statuses;
        }

        void SaveResult(const CheckerResult &result, const SiteStatus &status) override {
            if (crash_on_save) {
                crash_on_save = false;
                throw SimulatedCrash{}; // транзакция не закоммичена: «БД» не меняется
            }
            Log("save " + result.url);
            results.push_back(result);
            const bool notified =
                statuses.contains(result.url) ? statuses[result.url].notified_is_up : true;
            statuses[result.url] = status;
            statuses[result.url].notified_is_up = notified;
        }

        void MarkNotified(const std::string &url, bool is_up) override {
            if (crash_on_mark) {
                crash_on_mark = false;
                throw SimulatedCrash{};
            }
            Log("mark " + url + (is_up ? " up" : " down"));
            statuses[url].notified_is_up = is_up;
        }

        std::map<std::string, int> RecentFailures(std::chrono::seconds) override {
            ++recent_failures_calls;
            return recent_failures;
        }

    private:
        void Log(const std::string &entry) {
            if (journal_ != nullptr) {
                journal_->push_back(entry);
            }
        }

        std::vector<std::string> *journal_;
    };

} // namespace monitor::testing
