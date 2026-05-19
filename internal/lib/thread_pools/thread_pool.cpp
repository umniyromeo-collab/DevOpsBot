#pragma once

#include "thread_pool.h"

ThreadPool::ThreadPool(const size_t num_threads) : threads_num(num_threads) {}

void ThreadPool::Execute(const std::vector< std::function<void()> >& tasks) const {
    if (tasks.empty()) return;

    size_t cur_task_ind = 0;
    std::mutex index_mtx;
    std::vector<std::thread> threads;

    threads.reserve(threads_num);

    for (size_t i = 0; i < threads_num; ++i) {
        threads.emplace_back([&] {
            while (true) {
                size_t idx;
                {
                    std::lock_guard lock(index_mtx);
                    if (cur_task_ind >= tasks.size()) {
                        break;
                    }
                    idx = cur_task_ind++;
                }

                tasks[idx]();
            }
        });
    }

    for (auto &worker : threads) {
        worker.join();
    }
}