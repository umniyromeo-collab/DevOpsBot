#include "checker.h"

namespace HTTPSChecker {
    Checker::Checker(const Config &config, net::IFetcher &fetcher)
        : url_(config.url), http_code_(config.http_code), fetcher_(fetcher) {}

    // Код 0 — ответа не было вовсе (таймаут, DNS, отказ соединения): это тоже «недоступен».
    CheckerResult Checker::Check() const {
        const int code = fetcher_.FetchStatusCode(url_);
        return {url_, code, code == http_code_};
    }

    std::string Checker::URL() const {
        return this->url_;
    }
}
