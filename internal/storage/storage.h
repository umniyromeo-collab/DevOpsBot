#pragma once
#include "../status_checker/checkers/checker.h"

template<typename store_func>
class Storage {
public:
    ~Storage() = default;
    template <typename Entry>
    void Store(const Entry &res) {
        store_func_(res);
    }

private:
    store_func store_func_;
};
