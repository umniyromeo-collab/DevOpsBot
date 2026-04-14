#include <iostream>
#include <string>
// #include <curl/curl.h>


#include "checkers/builder/builder.h"
#include "checkers/http/config.h"
#include "checkers/https/config.h"


int main() {
    HTTPSChecker::Config httpsConfig{ "https://google.com", 200};
    Builder::Config config{Builder::ConfigType::https, httpsConfig};

    std::unique_ptr<IChecker> googleChecker = Builder::CreateChecker(config);

    std::cout << googleChecker->Check();
}