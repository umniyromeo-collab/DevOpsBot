#include "thread_queue.h"

#include <stdexcept>

template<typename T>
BoundedThreadSafeQueue<T>::BoundedThreadSafeQueue(size_t max_size) {
    if (max_size == 0) {
        throw std::invalid_argument("max_size == 0");
    }

    queue_.reserve(max_size);
}

template<typename T>
void BoundedThreadSafeQueue<T>::push(T value) {

    {
        std::unique_lock lock(mutex_);

        cv_full_.wait(lock, [this]{return queue_.size() != queue_.capacity();});

        queue_.push(std::move(value));
    }
}


template<typename T>
bool BoundedThreadSafeQueue<T>::try_push(T value) {

    if (queue_.size() == queue_.capacity()) {
        return false;
    }

    queue_.push(std::move(value));
    return true;
}

template<typename T>
bool BoundedThreadSafeQueue<T>::try_pop(T& out) {

    if (queue_.empty()) return false;

    out = std::move(queue_.front());
    queue_.pop();
    return true;
}

template<typename T>
void BoundedThreadSafeQueue<T>::wait_and_pop(T &out) {

    {
        std::unique_lock lock(mutex_);

        cv_full_.wait(lock, [this]{return !queue_.empty();});

        out = std::move(queue_.front());
        queue_.pop();
    }

}

template<typename T>
bool BoundedThreadSafeQueue<T>::empty() const {
    return queue_.empty();
}

template<typename T>
bool BoundedThreadSafeQueue<T>::full() const {
    return queue_.size() == queue_.capacity();
}

template<typename T>
size_t BoundedThreadSafeQueue<T>::size() const {
    return queue_.size();
}

template<typename T>
size_t BoundedThreadSafeQueue<T>::capacity() const {
    return queue_.capacity();
}

