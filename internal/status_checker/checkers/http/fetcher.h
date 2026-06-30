#pragma once
#include "Ifetcher.h"

#include <curl/curl.h>

// class FakeFetcher : public net::IFetcher {
// public:
//     int code_to_return = 0;
//     mutable std::string last_requested_url;
//
//     int FetchStatusCode(const std::string& url) const override {
//         last_requested_url = url;
//         return code_to_return;
//     }
// };

namespace HTTPChecker{
class Fetcher final : public net::IFetcher {

    Fetcher();

    int FetchStatusCode(const std::string &url) const override;

    ~Fetcher() override;

};


}