#pragma once
#include "config.h"
#include "fetcher.h"
#include "../checker.h"

namespace HTTPChecker {
    class Checker final : public IChecker {
    public:
        explicit Checker(const Config& config);

        explicit Checker(const Config &config, net::IFetcher &fetcher);

        virtual ~Checker() override {}
        bool Check() const override;
        std::string URL() const override;
        // HTTPSChecker (HTTPSChecker &&) noexcept = default;

    private:
        std::string url_;
        int http_code_;
        net::IFetcher* fetcher_;
    };
}