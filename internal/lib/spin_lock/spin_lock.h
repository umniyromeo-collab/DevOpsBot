#pragma once

#include <atomic>

class SpinLock {

public:
    SpinLock() = default;

    ~SpinLock() = default;

    void lock();

    void unlock();

private:
    std::atomic<bool> lock_flag;
};