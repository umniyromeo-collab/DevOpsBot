#include "checker.h"

namespace HTTPSChecker {
    Checker::Checker(const Config &config) : url_(config.url), http_code_(config.http_code) {}

    bool Checker::Check() const {
        return this->url_.starts_with("https://");
    }
}
