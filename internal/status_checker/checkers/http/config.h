#pragma once
#include <string>


namespace HTTPChecker {
    struct Config {
        std::string url;
        int http_code;
    };
}