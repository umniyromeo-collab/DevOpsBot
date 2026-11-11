#pragma once
#include "config.h"
#include "fetcher.h"
#include "../checker.h"

namespace HTTPChecker {
    class Checker final : public IChecker {
    public:
        // Конструктора без фетчера нет: с fetcher_ == nullptr Check() падал бы.
        // fetcher должен жить дольше чекера.
        explicit Checker(const Config &config, net::IFetcher &fetcher);

        virtual ~Checker() override {}
        CheckerResult Check() const override;
        std::string URL() const override;
        // HTTPSChecker (HTTPSChecker &&) noexcept = default;

    private:
        std::string url_;
        int http_code_;
        net::IFetcher* fetcher_;
    };
}
