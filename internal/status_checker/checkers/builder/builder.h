#pragma once

#include "config.h"
#include "../checker.h"

#include <memory>

namespace Builder{
    std::unique_ptr<IChecker> CreateChecker(const Config& url);
}