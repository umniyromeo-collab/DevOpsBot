#pragma once

#include <atomic>

class Semaphore {

public:
    Semaphore(int max_threads);

    ~Semaphore() = default;

    void add();

    void del();

private:
    const int max_threads;
    std::atomic<int> cnt_threads;
}