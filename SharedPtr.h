//
// Created by User on 24.02.2026.
//

#ifndef DEVOPSBOT_SHAREDPTR_H
#define DEVOPSBOT_SHAREDPTR_H

template<typename T>
class SharedPtr {
public:
    explicit SharedPtr() noexcept : m_ptr(nullptr), m_ref_count(nullptr) {
    };

    explicit SharedPtr(T *const ptr) noexcept : m_ptr(ptr), m_ref_count(new long(1)) {
    };

    SharedPtr(const SharedPtr &other) noexcept : m_ptr(other.m_ptr), m_ref_count(++(*other.m_ref_count)) {
    };

    SharedPtr &operator=(const SharedPtr &other) {
        if (this == &other) return *this;
        m_ptr = other.m_ptr;
        m_ref_count = &(--(*other.m_ref_count)); //может тут nullptr?
        return *this;
    };

    SharedPtr(SharedPtr&& other) noexcept : m_ptr(other.m_ptr), m_ref_count(other.m_ptr) {}
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this == &other) return *this;
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        m_ref_count = other.m_ref_count;
        other.m_ref_count = nullptr;
        return *this;
    }

    void reset(T* ptr = nullptr) noexcept {
        release();
        m_ptr = ptr;
        m_ref_count = new long(1);
    }
    void swap(SharedPtr &other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_ref_count, other.m_ref_count);
    }

    ~SharedPtr() {
        if (m_ref_count != nullptr && *m_ref_count == 1) {
            delete m_ptr;
            delete m_ref_count;
            return;
        }
        if (m_ref_count != nullptr) {
            --(*m_ref_count);
        }
    }

    T* get() const noexcept {
        return m_ptr;
    }

    T& operator->() const noexcept {
        return *m_ptr;
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }


#endif //DEVOPSBOT_SHAREDPTR_H