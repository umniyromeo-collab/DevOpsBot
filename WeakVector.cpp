//
// Created by User on 24.02.2026.
//

#include "WeakVector.h"

WeakVector::WeakVector() = default;

WeakVector::WeakVector(const size_t size) : size_(size), cap_(size), begin_(new int[size]) {
};

WeakVector::WeakVector(const WeakVector &other) : size_(other.size_), cap_(other.cap_), begin_(new int[other.cap_]) {
    memcpy(begin_, other.begin_, size_ * sizeof(int));
}

const WeakVector& WeakVector::operator=(WeakVector other) {
    size_ = other.size_;
    cap_ = other.cap_;
    begin_ = other.begin_;
    return *this;
}

WeakVector::WeakVector(WeakVector&& other): size_(other.size_), cap_(other.cap_), begin_(other.begin_) {
    other.size_ = 0;
    other.cap_ = 0;
    other.begin_ = nullptr;
}

WeakVector::~WeakVector() {
    delete[] begin_;
}

void WealVector::push_back(const int value) {
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

