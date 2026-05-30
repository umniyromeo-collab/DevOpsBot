#pragma once

#include <condition_variable>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>

class ThreadPool {
private:
    size_t threads_num;
    std::vector<std::thread> threads;

    std::queue< std::function < void() > > tasks;
    std::mutex task_mtx;

    bool stop_flag;
    std::condition_variable sleep_cv;

public:
    explicit ThreadPool(size_t num_threads);


    void Execute(std::function < void() > new_task);
};