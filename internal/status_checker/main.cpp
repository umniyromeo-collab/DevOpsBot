#include <iostream>
#include <string>
#include <thread>
#include <random>
#include <chrono>
#include <mutex>
// #include <curl/curl.h>


#include "checkers/builder/builder.h"
#include "checkers/http/config.h"
#include "checkers/https/config.h"


void process(const IChecker &checker, std::mutex &mtx) {
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::lock_guard lock(mtx);

    std::cout << checker.URL() << std::endl;

}

int main() {
    HTTPSChecker::Config httpsConfig{ "https://google.com", 200};
    const Builder::Config config{Builder::ConfigType::https, httpsConfig};

    const std::unique_ptr<IChecker> googleChecker = Builder::CreateChecker(config);

    std::cout << googleChecker->Check() << std::endl;

    std::vector<std::thread> Threads;

    const std::vector<Builder::Config> ThreadConfigs{
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google.com", 200}}
    };

    std::mutex mtx;

    for (const auto &thread_config : ThreadConfigs) {
        Threads.emplace_back(process, Builder::CreateChecker(thread_config), std::ref(mtx));
    }

    for (auto &thread : Threads) {
        thread.join();
    }
}