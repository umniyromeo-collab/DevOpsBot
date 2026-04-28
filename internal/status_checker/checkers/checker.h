#pragma once

class IChecker{
public:
    virtual ~IChecker() = default;
    virtual bool Check() const = 0;
};