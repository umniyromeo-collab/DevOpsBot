#pragma once

#include "../http/config.h"
#include "../https/config.h"

#include <string>
#include <variant>

namespace Builder {

    enum class ConfigType {http, https};

    struct Config {
        ConfigType type;
        std::variant<HTTPChecker::Config, HTTPSChecker::Config> config;
    };
}