#include "spin_lock.h"

void SpinLock::lock() {

    bool is_locked = false;

    while (!lock_flag.compare_exchange_strong(is_locked, true)) {}

}

void SpinLock::unlock() {

    lock_flag.store(false);
}
