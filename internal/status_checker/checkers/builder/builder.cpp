#include "builder.h"
#include "config.h"

#include "../http/checker.h"
#include "../https/checker.h"

#include <unordered_map>

namespace Builder {

    std::unordered_map<std::string, ConfigType> StrToType = {
        {"http", ConfigType::http}, {"https", ConfigType::https}
    };

    std::unique_ptr<IChecker> CreateChecker(const Config config) {
        ConfigType type = StrToType[config.type];

        switch (type) {
            case ConfigType::http : {
                HTTPChecker::Config checker_config = std::get<HTTPChecker::Config>;
                return std::make_unique<HTTPChecker::Checker>(checker_config);
            }

            case ConfigType::https : {
                HTTPChecker::Config checker_config = std::get<HTTPSChecker::Config>;
                return std::make_unique<HTTPSChecker::Checker>(checker_config);
            }
        }
    }
}
