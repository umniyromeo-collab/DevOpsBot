#pragma once
#include "config.h"
#include "../checker.h"

namespace HTTPSChecker {
    class Checker final : public IChecker {
    public:
        explicit Checker(const Config& config);
        virtual ~Checker() override {}
        bool Check() const override;
        std::string URL() const override;
        // Checker (Checker &&) noexcept = default;
    private:
        std::string url_;
        int http_code_;
    };
}
