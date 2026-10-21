#include <iostream>
#include <string>
#include <thread>
#include <random>
#include <chrono>
#include <mutex>
#include <functional>
// #include <curl/curl.h>


#include "checkers/builder/builder.h"
#include "checkers/http/config.h"
#include "checkers/https/config.h"
#include "../lib/thread_pools/thread_pool.h"

void process(const IChecker &checker, std::mutex &mtx) {
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::lock_guard<std::mutex> lock(mtx);

    std::cout << checker.URL() << std::endl;
}

int main() {
    HTTPSChecker::Config httpsConfig{"https://google.com", 200};
    const Builder::Config config{Builder::ConfigType::https, httpsConfig};

    const std::unique_ptr<IChecker> googleChecker = Builder::CreateChecker(config);

    std::cout << googleChecker->Check().is_up << std::endl;

    std::vector<std::thread> Threads;

    const std::vector<Builder::Config> ThreadConfigs{
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google1.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google2.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google3.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google4.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google5.com", 200}},
        {Builder::ConfigType::https, HTTPSChecker::Config{"https://google6.com", 200}}
    };

    std::mutex pool_mtx;
    ThreadPool pool(4);

    for (int _ = 0; _ < 5; ++_) {
        std::cout << "start" << std::endl;


        for (const auto &thread_config : ThreadConfigs) {
            auto checker = std::shared_ptr<IChecker>(Builder::CreateChecker(thread_config));

           pool.Execute([checker, &pool_mtx]() {
                process(*checker, pool_mtx);
           });
        }

        std::cout << "end" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
