#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <mutex>

class ThreadPool {
private:
    size_t threads_num;

public:
    explicit ThreadPool(size_t num_threads);

    void Execute(const std::vector< std::function<void()> >& tasks) const;
};