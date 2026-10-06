#pragma once
#include <string>

#include "checker_result.h"

class IChecker{
public:
    virtual ~IChecker() = default;
    [[nodiscard]] virtual CheckerResult Check() const = 0;
    [[nodiscard]] virtual std::string URL() const = 0;
};
