#include "builder.h"
#include "config.h"

#include "../http/checker.h"
#include "../https/checker.h"


namespace Builder {

    std::unique_ptr<IChecker> CreateChecker(const Config& config) {
        ConfigType type = config.type;

        switch (type) {
            case ConfigType::http : {
                auto checker_config = std::get<HTTPChecker::Config>(config.config);
                return std::make_unique<HTTPChecker::Checker>(checker_config);
            }

            case ConfigType::https : {
                auto checker_config = std::get<HTTPSChecker::Config>(config.config);
                return std::make_unique<HTTPSChecker::Checker>(checker_config);
            }

            default : {
                return nullptr;
            }
        }
    }
}
