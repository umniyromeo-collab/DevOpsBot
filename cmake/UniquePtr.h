//
// Created by User on 24.02.2026.
//

#ifndef DEVOPSBOT_UNIQUEPTR_H
#define DEVOPSBOT_UNIQUEPTR_H

template<typename T>
class UniquePtr {
public:
    explicit UniquePtr() noexcept : m_ptr(nullptr) {
    };

    explicit UniquePtr(const T &value) noexcept : m_ptr(new T(value)) {
    };

    explicit UniquePtr(T *const ptr) : m_ptr(ptr) {
    };

    ~UniquePtr() {
        delete m_ptr;
    };

    UniquePtr(const UniquePtr &other) = delete;

    UniquePtr &operator=(const UniquePtr &other) = delete;

    T *release() noexcept {
        return m_ptr;
    }

    void reset(T *const ptr = nullptr) noexcept {
        delete m_ptr;
        m_ptr = ptr;
    }

    T &operator*() const {
        return *m_ptr;
    }

    T *operator->() const noexcept {
        return m_ptr;
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

private:
    T *const m_ptr;
};


#endif //DEVOPSBOT_UNIQUEPTR_H