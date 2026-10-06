#pragma once

#include <cstdlib>

#include <pqxx/pqxx>

#include "../storage/storage.h"

namespace PG {

    auto PGStore = [](const CheckerResult &res) -> void {

    static const char *url = std::getenv("DATABASE_URL");
    static pqxx::connection conn{url ? url : ""};

    pqxx::work tx{conn};
    tx.exec("INSERT INTO check_results (url, http_code, is_up) VALUES ($1, $2, $3)",
            pqxx::params{res.url, res.http_code, res.is_up});
    tx.commit();
};

using PGStore_t = decltype(PGStore);
using PGStorage = Storage<PGStore_t>;

}
