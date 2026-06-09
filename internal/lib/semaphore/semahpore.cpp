#include "semaphore.h"

Semaphore::Semaphore(const int max_threads_) : max_threads(max_threads_), cnt_threads(0) {}

void Semaphore::add() {

    int cnt = cnt_threads;

    while (true) {

        if (cnt < max_threads && cnt_threads.compare_exchange_strong(cnt, cnt + 1 )) {
            return;
        }

    }
}

void Semaphore::del() {

}


