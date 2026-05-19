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

    std::mutex pool_mtx;
    const ThreadPool pool(4);

    while (true) {
        std::cout << "start" << std::endl;

        std::vector< std::function<void()> > tasks;

        for (const auto &thread_config : ThreadConfigs) {
            auto checker = std::shared_ptr<IChecker>(Builder::CreateChecker(thread_config));

           tasks.emplace_back([checker, &pool_mtx]() {
                process(*checker, pool_mtx);
           });
        }

        pool.Execute(tasks);

        std::cout << "end" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
