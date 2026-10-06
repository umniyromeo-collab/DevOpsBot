#pragma once
#include <string>

struct CheckerResult {
    std::string url;
    int http_code;
    bool is_up;
};
