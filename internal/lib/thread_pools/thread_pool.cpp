#include "thread_pool.h"

ThreadPool::ThreadPool(const size_t num_threads) : threads_num(num_threads), stop_flag(false) {
    threads.reserve(num_threads);

    for (size_t i = 0; i < threads_num; ++i) {
        threads.emplace_back([&] {

            while (true) {

                std::unique_lock lock(task_mtx);

                // while (tasks.empty()) {
                //     sleep_cv.wait(lock);
                // }

                sleep_cv.wait(lock, [&] { return stop_flag || !tasks.empty(); });
                // более короткая запись ^

                if (stop_flag && tasks.empty()) {
                    return;
                }

                auto task = std::move(tasks.front());

                tasks.pop();

                lock.unlock();
                task();

            }

        });
    }
}

void ThreadPool::Execute(std::function < void() > new_task) {

    std::lock_guard lock(task_mtx);

    tasks.push(std::move(new_task));
    sleep_cv.notify_one();

}

ThreadPool::~ThreadPool(){

}
