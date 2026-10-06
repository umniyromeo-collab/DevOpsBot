#pragma once
#include "../status_checker/checkers/checker.h"

template<typename store_func>
class Storage {
public:
  virtual ~Storage() = default;
  virtual void Store(CheckerResult &res) {store_func_(res);}
private:
  store_func store_func_;
};
