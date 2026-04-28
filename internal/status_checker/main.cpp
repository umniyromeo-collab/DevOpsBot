#include <iostream>
#include <string>
#include <curl/curl.h>


#include "chekers/builder/builder.h"
#include "chekers/http/config.h"
#include "chekers/https/config.h"


int main() {
    std::string s = "https://google.com";
    HTTPSConfig::Config config{s, 200};
    Builder::Config config{ConfigType::https, config};

    std::unique_ptr<IChecker> googleChecker = Builder::CreateChecker(config);

    std::cout << googleChecker->Check();
}