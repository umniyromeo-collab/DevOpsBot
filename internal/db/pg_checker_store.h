#pragma once

#include "../storage/ichecker_store.h"

#include <pqxx/pqxx>

#include <string>

namespace PG {

    // Хранилище чекера на Postgres. Соединение у каждого объекта своё: pqxx::connection
    // не потокобезопасен, поэтому каждому потоку, который ходит в БД, — свой CheckerStore.
    // Переподключения нет: при обрыве методы бросают исключение, и процесс завершается.
    class CheckerStore final : public storage::ICheckerStore {
    public:
        explicit CheckerStore(const std::string &database_url);

        std::vector<storage::Site> LoadSites() override;
        std::map<std::string, monitor::SiteStatus> LoadStatuses() override;
        void SaveResult(const CheckerResult &result, const monitor::SiteStatus &status) override;
        void MarkNotified(const std::string &url, bool is_up) override;
        std::map<std::string, int> RecentFailures(std::chrono::seconds window) override;

    private:
        pqxx::connection conn_;
    };

} // namespace PG
