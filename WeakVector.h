//
// Created by User on 24.02.2026.
//

#ifndef DEVOPSBOT_WEAKVECTOR_H
#define DEVOPSBOT_WEAKVECTOR_H


class WeakVector {
public:
    WeakVector() = default;
    explicit WeakVector(const size_t size);
    WeakVector(const WeakVector &other);
    const WeakVector& operator=(WeakVector other);
    WeakVector(WeakVector &&other);
    ~WeakVector();
    void push_back(const int value);

private:
    size_t size_;
    size_t cap_;
    int *begin_;
};


#endif //DEVOPSBOT_WEAKVECTOR_H