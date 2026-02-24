//
// Created by User on 24.02.2026.
//

#ifndef DEVOPSBOT_WEAKVECTOR_H
#define DEVOPSBOT_WEAKVECTOR_H


class WeakVector {
public:
    WeakVector() = default;

    explicit WeakVector(const size_t size) : size_(size), cap_(size), begin_(new int[size]) {
    };

    WeakVector(const WeakVector &other) : size_(other.size_), cap_(other.cap_), begin_(new int[other.cap_]) {
        memcpy(begin_, other.begin_, size_ * sizeof(int));
    }

    WeakVector &operator=(WeakVector other) {
        size_ = other.size_;
        cap_ = other.cap_;
        std::swap(begin_, other.begin_);
        return *this;
    }

    ~WeakVector() {
        delete[] begin_;
    }

    void push_back(const int value) {
        if (size_ == cap_) {
            WeakVector temp(2 * cap_);
            memcpy(temp.begin_, begin_, size_ * sizeof(int));
            temp.begin_[size_] = value;
            cap_ *= 2;
            std::swap(begin_, temp.begin_);
        }
        begin_[size_] = value;
        ++size_;
    }

private:
    size_t size_;
    size_t cap_;
    int *begin_;
};


#endif //DEVOPSBOT_WEAKVECTOR_H