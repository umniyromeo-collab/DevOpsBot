#pragma once

class IChecker{
public:
    virtual ~IChecker() = default;
    [[nodiscard]] virtual bool Check() const = 0;
    [[nodiscard]] virtual std::string URL() const = 0;
};

