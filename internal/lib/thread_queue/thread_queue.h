#pragma once

#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

template <typename T>
class BoundedThreadSafeQueue {
public:
    // Создаёт очередь ёмкостью max_size. max_size == 0 запрещено.
    explicit BoundedThreadSafeQueue(size_t max_size);
    ~BoundedThreadSafeQueue() = default;

    BoundedThreadSafeQueue(const BoundedThreadSafeQueue&) = delete;
    BoundedThreadSafeQueue& operator=(const BoundedThreadSafeQueue&) = delete;

    // Блокирующее добавление. Засыпает на cv, пока в очереди max_size элементов.
    // Будит одного ожидающего в wait_and_pop после успешной вставки.
    void push(T value);

    // Неблокирующее добавление. Возвращает false, если очередь полная.
    bool try_push(T value);

    // Неблокирующее извлечение. Возвращает false, если очередь пустая.
    bool try_pop(T& out);

    // Блокирующее извлечение. Засыпает на cv, пока очередь пустая.
    // Будит одного ожидающего в push после успешного извлечения.
    void wait_and_pop(T& out);

    // Снимки состояния. В многопоточной среде значения устаревают мгновенно —
    // использовать только для логов / метрик, не как условие безопасности.
    bool empty() const;
    bool full() const;
    size_t size() const;
    size_t capacity() const;

private:
    std::queue<T> queue_;

    std::mutex mutex_;
    std::condition_variable cv_full_;
    std::condition_variable cv_empty;
};
