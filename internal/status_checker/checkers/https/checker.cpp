#include "checker.h"

namespace HTTPSChecker {
    Checker::Checker(const Config &config) : url_(config.url), http_code_(config.http_code) {}

    // В сеть пока не ходит, поэтому реального кода ответа нет — пишем 0.
    CheckerResult Checker::Check() const {
        return {url_, 0, this->url_.find("https://") == 0};
    }

    std::string Checker::URL() const {
        return this->url_;
    }
}
