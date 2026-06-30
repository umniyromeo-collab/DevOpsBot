#include "checker.h"

namespace HTTPChecker{
    Checker::Checker(const Config &config) : url_(config.url), http_code_(config.http_code), fetcher_(nullptr) {}
    Checker::Checker(const Config &config, net::IFetcher &fetcher) {
        fetcher_ = &fetcher;
    }


    bool Checker::Check() const {
        return this->url_.find("http://") == 0;
    }

    std::string Checker::URL() const {
        return this->url_;
    }
}

