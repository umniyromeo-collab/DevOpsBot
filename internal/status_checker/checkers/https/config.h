#pragma once
#include <string>

namespace HTTPSChecker {
    struct Config {
        std::string url;
        int http_code;
    };
}