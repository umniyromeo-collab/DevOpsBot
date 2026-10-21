#pragma once

#include <cstdlib>
#include <format>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <pqxx/pqxx>

#include "../storage/storage.h"

namespace PG {

    using DBValue = std::variant<std::monostate, std::string, int, long long, double, bool>;

    struct DBEntry {
        std::string table;
        std::vector<std::pair<std::string, DBValue>> fields;
    };

    inline const auto PGStore = [](const DBEntry &entry) -> void {
        if (entry.fields.empty()) {
            throw std::invalid_argument("PGStore: DBEntry has no fields");
        }

        static const char *url = std::getenv("DATABASE_URL");
        static pqxx::connection conn{url ? url : ""};

        pqxx::work tx{conn};

        std::string columns;
        std::string placeholders;
        pqxx::params params;
        for (std::size_t i = 0; i < entry.fields.size(); ++i) {
            const auto &[name, value] = entry.fields[i];

            if (i != 0) {
                columns += ", ";
                placeholders += ", ";
            }

            columns += tx.quote_name(name);
            placeholders += std::format("${}", i + 1);

            std::visit(
                [&params]<typename T>(const T &v) {
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        params.append();
                    } else {
                        params.append(v);
                    }
                },
                value);
        }

        tx.exec(std::format("INSERT INTO {} ({}) VALUES ({})", tx.quote_name(entry.table), columns,
                            placeholders),
                params);
        tx.commit();
    };

    using PGStore_t = decltype(PGStore);
    using PGStorage = Storage<PGStore_t>;
}
