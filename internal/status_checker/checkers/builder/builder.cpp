#include "builder.h"
#include "config.h"

#include "../http/checker.h"
#include "../https/checker.h"


namespace Builder {

    std::optional<Config> ConfigFromSite(const std::string &url, int expected_http_code) {
        if (url.starts_with("https://")) {
            return Config{ConfigType::https, HTTPSChecker::Config{url, expected_http_code}};
        }
        if (url.starts_with("http://")) {
            return Config{ConfigType::http, HTTPChecker::Config{url, expected_http_code}};
        }
        return std::nullopt;
    }

    std::unique_ptr<IChecker> CreateChecker(const Config &config, net::IFetcher &fetcher) {
        ConfigType type = config.type;

        switch (type) {
            case ConfigType::http : {
                auto checker_config = std::get<HTTPChecker::Config>(config.config);
                return std::make_unique<HTTPChecker::Checker>(checker_config, fetcher);
            }

            case ConfigType::https : {
                auto checker_config = std::get<HTTPSChecker::Config>(config.config);
                return std::make_unique<HTTPSChecker::Checker>(checker_config, fetcher);
            }

            default : {
                return nullptr;
            }
        }
    }
}
