#include "fetcher.h"

#include <cpr/cpr.h>

namespace net {

    int CprFetcher::FetchStatusCode(const std::string& url) const {
        const cpr::Response response = cpr::Get(
            cpr::Url{url},
            cpr::Timeout{10000}  // 10 секунд, чтобы не висеть вечно
        );
        // cpr сам идёт за редиректами. При сетевой ошибке (таймаут, DNS, отказ)
        // status_code == 0 — ровно то, что наш Check() трактует как "недоступен".
        return static_cast<int>(response.status_code);
    }

}  // namespace net