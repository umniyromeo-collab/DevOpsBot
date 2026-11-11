#include "scheduler.h"

#include "fake_checker_store.h"
#include "priority.h"
#include "priority_updater.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using monitor::Scheduler;

namespace {

    // Чекер без сети: возвращает заданную доступность и считает вызовы.
    class FakeChecker final : public IChecker {
    public:
        explicit FakeChecker(std::string url) : url_(std::move(url)) {}

        bool is_up = true;
        bool throws = false;
        mutable std::atomic<int> calls{0};

        CheckerResult Check() const override {
            ++calls;
            if (throws) {
                throw std::runtime_error("boom");
            }
            return {url_, is_up ? 200 : 503, is_up};
        }

        std::string URL() const override {
            return url_;
        }

    private:
        std::string url_;
    };

    // «Пул», который не выполняет задачи сам: тест решает, когда их запустить.
    struct Harness {
        std::vector<Scheduler::Task> tasks;
        std::vector<CheckerResult> results;
        bool should_run = true;
        Scheduler::Clock::time_point start = Scheduler::Clock::now();

        Scheduler Make(std::vector<Scheduler::Site> sites) {
            return Scheduler(
                std::move(sites),
                [this](Scheduler::Task task) { tasks.push_back(std::move(task)); },
                [this](CheckerResult result) {
                    results.push_back(std::move(result));
                    return true;
                },
                [this] { return should_run; }, start);
        }

        void RunAll() {
            auto pending = std::move(tasks);
            tasks.clear();
            for (auto &task : pending) {
                task();
            }
        }
    };

} // namespace

TEST(Scheduler, FirstTickSubmitsEverySiteOnce) {
    Harness h;
    auto a = std::make_shared<FakeChecker>("https://a");
    auto b = std::make_shared<FakeChecker>("https://b");
    auto scheduler = h.Make({{a, 60s}, {b, 30s}});

    const auto next = scheduler.Tick(h.start);
    EXPECT_EQ(h.tasks.size(), 2u);
    EXPECT_EQ(next, h.start + 30s);

    h.RunAll();
    EXPECT_EQ(h.results.size(), 2u);

    scheduler.Tick(h.start + 1s); // срок ещё не подошёл
    EXPECT_TRUE(h.tasks.empty());
}

// specs/site-monitoring: «Одна проверка сайта одновременно».
TEST(Scheduler, SkipsSiteWhileItsCheckIsInFlight) {
    Harness h;
    auto a = std::make_shared<FakeChecker>("https://a");
    auto scheduler = h.Make({{a, 60s}});

    scheduler.Tick(h.start);
    ASSERT_EQ(h.tasks.size(), 1u); // задача поставлена, но ещё не выполнена

    scheduler.Tick(h.start + 60s);
    scheduler.Tick(h.start + 120s);
    EXPECT_EQ(h.tasks.size(), 1u) << "задачи сайта не должны копиться";

    h.RunAll(); // проверка завершилась, результат отдан на запись
    scheduler.Tick(h.start + 180s);
    EXPECT_EQ(h.tasks.size(), 1u);
    EXPECT_EQ(a->calls, 1);
}

TEST(Scheduler, FailedCheckDoesNotEscapeTaskAndReleasesSite) {
    Harness h;
    auto a = std::make_shared<FakeChecker>("https://a");
    a->throws = true;
    auto scheduler = h.Make({{a, 60s}});

    scheduler.Tick(h.start);
    EXPECT_NO_THROW(h.RunAll());
    EXPECT_TRUE(h.results.empty());

    scheduler.Tick(h.start + 60s);
    EXPECT_EQ(h.tasks.size(), 1u) << "после исключения сайт снова проверяется";
}

TEST(Scheduler, StoppedTaskSkipsNetworkAndReleasesSite) {
    Harness h;
    auto a = std::make_shared<FakeChecker>("https://a");
    auto scheduler = h.Make({{a, 60s}});

    scheduler.Tick(h.start);
    h.should_run = false;
    h.RunAll();
    EXPECT_EQ(a->calls, 0);
    EXPECT_TRUE(h.results.empty());

    scheduler.Tick(h.start + 60s);
    EXPECT_EQ(h.tasks.size(), 1u);
}

// specs/site-monitoring: «Учащённый опрос после сбоев» — срок пересчитывается от последнего
// запуска.
TEST(Scheduler, FailingSiteIsCheckedSooner) {
    Harness h;
    auto a = std::make_shared<FakeChecker>("https://a");
    auto scheduler = h.Make({{a, 60s}});
    scheduler.Tick(h.start);
    h.RunAll();

    scheduler.UpdateIntervals({{"https://a", 1}});
    EXPECT_EQ(scheduler.IntervalOf("https://a"), 15s);

    scheduler.Tick(h.start + 15s);
    EXPECT_EQ(h.tasks.size(), 1u);

    scheduler.UpdateIntervals({});
    EXPECT_EQ(scheduler.IntervalOf("https://a"), 60s);
}

// 9.2: пересчёт раз в RECALC_PERIOD по истории из хранилища.
TEST(PriorityUpdater, RecalculatesOncePerPeriod) {
    Harness h;
    auto a = std::make_shared<FakeChecker>("https://a");
    auto scheduler = h.Make({{a, 60s}});
    monitor::testing::FakeCheckerStore store;
    store.recent_failures = {{"https://a", 2}};
    monitor::PriorityUpdater updater(store, h.start);

    EXPECT_TRUE(updater.MaybeUpdate(h.start, scheduler));
    EXPECT_EQ(store.recent_failures_calls, 1);
    EXPECT_EQ(scheduler.IntervalOf("https://a"), 15s);

    EXPECT_FALSE(updater.MaybeUpdate(h.start + 10s, scheduler));
    EXPECT_EQ(store.recent_failures_calls, 1);

    store.recent_failures.clear();
    EXPECT_TRUE(updater.MaybeUpdate(h.start + monitor::RECALC_PERIOD, scheduler));
    EXPECT_EQ(store.recent_failures_calls, 2);
    EXPECT_EQ(scheduler.IntervalOf("https://a"), 60s);
}
