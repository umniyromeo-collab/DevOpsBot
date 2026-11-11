#pragma once

#include "../status_checker/monitor/site_status.h"
#include "checker_result.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace storage {

    // Сайт из фиксированного списка.
    struct Site {
        std::string url;
        std::chrono::seconds base_interval;
        int expected_http_code;
    };

    // Хранилище процесса чекера. В заголовке нет libpqxx: логика зависит только от интерфейса,
    // а тесты подменяют его фейком. Объект не потокобезопасен — у каждого потока свой.
    class ICheckerStore {
    public:
        virtual ~ICheckerStore() = default;

        // Фиксированный список сайтов.
        [[nodiscard]] virtual std::vector<Site> LoadSites() = 0;

        // Создаёт недостающие строки состояния (статус «работает») и возвращает состояние
        // всех сайтов, ключ — URL.
        [[nodiscard]] virtual std::map<std::string, monitor::SiteStatus> LoadStatuses() = 0;

        // Одной транзакцией: строка в check_results и новое состояние сайта. notified_is_up
        // не меняется — его двигает только MarkNotified после подтверждения Kafka.
        virtual void SaveResult(const CheckerResult &result, const monitor::SiteStatus &status) = 0;

        // Kafka подтвердила приём события «сайт в статусе is_up».
        virtual void MarkNotified(const std::string &url, bool is_up) = 0;

        // Число неудачных проверок каждого сайта за последнее окно. Сайтов без неудач в ответе нет.
        [[nodiscard]] virtual std::map<std::string, int>
        RecentFailures(std::chrono::seconds window) = 0;
    };

} // namespace storage
