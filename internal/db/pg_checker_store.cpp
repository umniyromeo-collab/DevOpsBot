#include "pg_checker_store.h"

#include <cstdint>

namespace PG {

    CheckerStore::CheckerStore(const std::string &database_url) : conn_(database_url) {}

    std::vector<storage::Site> CheckerStore::LoadSites() {
        pqxx::read_transaction tx{conn_};
        std::vector<storage::Site> sites;
        for (const auto &row : tx.exec("SELECT url, base_interval_sec, expected_http_code "
                                       "FROM sites ORDER BY url")) {
            sites.push_back({row[0].as<std::string>(), std::chrono::seconds{row[1].as<int>()},
                             row[2].as<int>()});
        }
        return sites;
    }

    std::map<std::string, monitor::SiteStatus> CheckerStore::LoadStatuses() {
        pqxx::work tx{conn_};
        // Новый сайт начинает со статуса «работает», о котором «уже сообщили»: первая успешная
        // проверка не будет сменой статуса, а первое падение — будет.
        tx.exec("INSERT INTO site_state (url, is_up, notified_is_up) "
                "SELECT url, true, true FROM sites "
                "ON CONFLICT (url) DO NOTHING");

        // Код последней проверки нужен событию, если его досылают после перезапуска.
        // Подзапрос идёт по индексу check_results (url, checked_at DESC).
        const pqxx::result rows =
            tx.exec("SELECT s.url, s.is_up, s.notified_is_up, s.consecutive_failures, "
                    "       extract(epoch FROM s.changed_at)::bigint, "
                    "       COALESCE((SELECT c.http_code FROM check_results c WHERE c.url = s.url "
                    "                 ORDER BY c.checked_at DESC LIMIT 1), 0) "
                    "FROM site_state s JOIN sites USING (url)");
        tx.commit();

        std::map<std::string, monitor::SiteStatus> statuses;
        for (const auto &row : rows) {
            monitor::SiteStatus status;
            status.is_up = row[1].as<bool>();
            status.notified_is_up = row[2].as<bool>();
            status.consecutive_failures = row[3].as<int>();
            status.changed_at = row[4].as<std::int64_t>();
            status.last_http_code = row[5].as<int>();
            statuses.emplace(row[0].as<std::string>(), status);
        }
        return statuses;
    }

    void CheckerStore::SaveResult(const CheckerResult &result, const monitor::SiteStatus &status) {
        pqxx::work tx{conn_};
        tx.exec("INSERT INTO check_results (url, http_code, is_up) VALUES ($1, $2, $3)",
                pqxx::params{result.url, result.http_code, result.is_up});
        // UPSERT, а не UPDATE: строка состояния появится, даже если LoadStatuses её не создал.
        // notified_is_up у новой строки — true (начальный статус «работает»), у существующей
        // не меняется.
        tx.exec(
            "INSERT INTO site_state (url, is_up, notified_is_up, consecutive_failures, changed_at) "
            "VALUES ($1, $2, true, $3, to_timestamp($4::double precision)) "
            "ON CONFLICT (url) DO UPDATE SET is_up = EXCLUDED.is_up, "
            "    consecutive_failures = EXCLUDED.consecutive_failures, "
            "    changed_at = EXCLUDED.changed_at",
            pqxx::params{result.url, status.is_up, status.consecutive_failures, status.changed_at});
        tx.commit();
    }

    void CheckerStore::MarkNotified(const std::string &url, bool is_up) {
        pqxx::work tx{conn_};
        tx.exec("UPDATE site_state SET notified_is_up = $2 WHERE url = $1",
                pqxx::params{url, is_up});
        tx.commit();
    }

    std::map<std::string, int> CheckerStore::RecentFailures(std::chrono::seconds window) {
        pqxx::read_transaction tx{conn_};
        std::map<std::string, int> failures;
        for (const auto &row : tx.exec("SELECT url, count(*)::int FROM check_results "
                                       "WHERE NOT is_up AND checked_at > now() - "
                                       "make_interval(secs => $1::double precision) "
                                       "GROUP BY url",
                                       pqxx::params{static_cast<std::int64_t>(window.count())})) {
            failures.emplace(row[0].as<std::string>(), row[1].as<int>());
        }
        return failures;
    }

} // namespace PG
