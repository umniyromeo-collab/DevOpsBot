#include "checker.h"

namespace HTTPChecker{
    Checker::Checker(const Config &config) : url_(config.url), http_code_(config.http_code) {}

    bool Checker::Check() const {
        return this->url_.starts_with("http://");
    }

    std::string Checker::URL() const {
        return this->url_;
    }
}

