#pragma once

#include "../../storage/ichecker_store.h"
#include "scheduler.h"

namespace monitor {

    // Раз в RECALC_PERIOD читает из БД число неудач за FAILURE_WINDOW и пересчитывает интервалы
    // планировщика. Работает в потоке планировщика со своим хранилищем (своим соединением).
    class PriorityUpdater {
    public:
        // Первый пересчёт — сразу: сайт со сбоями до перезапуска сразу опрашивается чаще.
        PriorityUpdater(storage::ICheckerStore &store, Scheduler::Clock::time_point start)
            : store_(store), next_(start) {}

        // true — пересчитал. Ошибки БД пробрасывает наружу.
        bool MaybeUpdate(Scheduler::Clock::time_point now, Scheduler &scheduler);

    private:
        storage::ICheckerStore &store_;
        Scheduler::Clock::time_point next_;
    };

} // namespace monitor
