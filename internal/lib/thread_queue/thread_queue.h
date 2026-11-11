#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>

// Ограниченная потокобезопасная очередь для схемы «производители — потребитель» (bounded buffer).
//
// Шаблон определён целиком в заголовке: компилятор должен видеть тела методов в месте
// инстанцирования, иначе линковщик не найдёт, например,
// BoundedThreadSafeQueue<CheckerResult>::push.
template <typename T>
class BoundedThreadSafeQueue {
public:
    // Создаёт очередь ёмкостью max_size. max_size == 0 запрещено: в такую очередь ничего
    // нельзя положить, и push заснул бы навсегда.
    explicit BoundedThreadSafeQueue(std::size_t max_size) : max_size_(max_size) {
        if (max_size_ == 0) {
            throw std::invalid_argument("BoundedThreadSafeQueue: max_size == 0");
        }
    }
    ~BoundedThreadSafeQueue() = default;

    BoundedThreadSafeQueue(const BoundedThreadSafeQueue&) = delete;
    BoundedThreadSafeQueue& operator=(const BoundedThreadSafeQueue&) = delete;

    // Блокирующее добавление. Засыпает, пока в очереди max_size элементов.
    // Будит одного ожидающего в wait_and_pop после успешной вставки.
    // Возвращает false, если очередь закрыта до или во время ожидания: значение не добавлено.
    [[nodiscard]] bool push(T value) {
        {
            std::unique_lock lock(mutex_);
            not_full_.wait(lock, [this] { return closed_ || queue_.size() < max_size_; });
            if (closed_) {
                return false;
            }
            queue_.push(std::move(value));
        }
        // Будим уже после unlock: иначе разбуженный поток сразу уснёт на занятом мьютексе.
        not_empty_.notify_one();
        return true;
    }

    // Неблокирующее добавление. Возвращает false, если очередь полная или закрыта.
    [[nodiscard]] bool try_push(T value) {
        {
            std::lock_guard lock(mutex_);
            if (closed_ || queue_.size() >= max_size_) {
                return false;
            }
            queue_.push(std::move(value));
        }
        not_empty_.notify_one();
        return true;
    }

    // Неблокирующее извлечение. Возвращает false, если очередь пустая.
    // После close() по-прежнему отдаёт оставшиеся элементы.
    [[nodiscard]] bool try_pop(T &out) {
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                return false;
            }
            out = std::move(queue_.front());
            queue_.pop();
        }
        not_full_.notify_one();
        return true;
    }

    // Блокирующее извлечение. Засыпает, пока очередь пустая и не закрыта.
    // Будит одного ожидающего в push после успешного извлечения.
    // Возвращает false, только когда очередь закрыта и пуста: так потребитель дочитывает
    // остаток после close() и лишь потом выходит.
    [[nodiscard]] bool wait_and_pop(T &out) {
        {
            std::unique_lock lock(mutex_);
            not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
            if (queue_.empty()) {
                return false;
            }
            out = std::move(queue_.front());
            queue_.pop();
        }
        not_full_.notify_one();
        return true;
    }

    // Закрывает очередь: push и try_push начинают возвращать false, ждущие просыпаются.
    // Повторный вызов безопасен.
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        // notify_all: на полной очереди могут ждать сразу все производители,
        // и каждый должен увидеть closed_ и выйти.
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    // Снимки состояния. В многопоточной среде значения устаревают мгновенно —
    // использовать только для логов / метрик, не как условие безопасности.
    [[nodiscard]] bool closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] bool full() const {
        std::lock_guard lock(mutex_);
        return queue_.size() >= max_size_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    // Ёмкость задаётся в конструкторе и не меняется, поэтому мьютекс не нужен.
    [[nodiscard]] std::size_t capacity() const {
        return max_size_;
    }

private:
    const std::size_t max_size_;
    std::queue<T> queue_;
    bool closed_ = false;

    // mutable: снимки состояния — const-методы, но им тоже нужна блокировка.
    mutable std::mutex mutex_;
    // Ждут производители: «в очереди появилось место».
    std::condition_variable not_full_;
    // Ждёт потребитель: «в очереди появился элемент».
    std::condition_variable not_empty_;
};
