#pragma once
#include "../Ifetcher.h"
#include "../checker.h"
#include "config.h"

namespace HTTPSChecker {
    // Ходит в сеть через net::IFetcher, как HTTPChecker: cpr одинаково работает с http и https,
    // а в тестах сеть подменяется тем же фейком.
    class Checker final : public IChecker {
    public:
        // fetcher должен жить дольше чекера: хранится ссылка.
        Checker(const Config &config, net::IFetcher &fetcher);
        virtual ~Checker() override {}
        CheckerResult Check() const override;
        std::string URL() const override;
        // Checker (Checker &&) noexcept = default;
    private:
        std::string url_;
        int http_code_;
        net::IFetcher &fetcher_;
    };
}
