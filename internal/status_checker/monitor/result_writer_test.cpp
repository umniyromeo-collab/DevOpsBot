#include "result_writer.h"

#include "fake_checker_store.h"
#include "fake_publisher.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace std::chrono_literals;
using monitor::ResultWriter;
using monitor::testing::FakeCheckerStore;
using monitor::testing::FakePublisher;
using monitor::testing::SimulatedCrash;

namespace {

    const std::string kUrl = "https://a.example";

    CheckerResult Up() {
        return {kUrl, 200, true};
    }

    CheckerResult Down() {
        return {kUrl, 503, false};
    }

    // «БД» и «брокер» переживают перезапуск процесса, а ResultWriter — нет: каждый тест создаёт
    // новый writer поверх тех же фейков, когда изображает рестарт.
    struct World {
        std::vector<std::string> journal;
        FakeCheckerStore store{&journal};
        FakePublisher publisher{&journal};

        World() {
            store.sites = {{kUrl, 60s, 200}};
        }

        ResultWriter StartWriter() {
            ResultWriter writer(store, publisher, [] { return std::int64_t{1000}; });
            writer.Recover();
            return writer;
        }

        // Сайт уже признан упавшим (две неудачи подряд).
        void MakeDown(ResultWriter &writer) {
            writer.Process(Down());
            writer.Process(Down());
        }
    };

} // namespace

// ---- 8.1: порядок вызовов ----

TEST(ResultWriter, StatusChangeGoesSaveThenPublishThenMark) {
    World world;
    auto writer = world.StartWriter();

    world.MakeDown(writer);

    const std::vector<std::string> expected{
        "save " + kUrl,
        "save " + kUrl,
        "publish " + kUrl + " down",
        "mark " + kUrl + " down",
    };
    EXPECT_EQ(world.journal, expected);
    ASSERT_EQ(world.publisher.events.size(), 1u);
    EXPECT_FALSE(world.publisher.events[0].is_up);
    EXPECT_EQ(world.publisher.events[0].http_code, 503);
    EXPECT_EQ(world.publisher.events[0].changed_at, 1000);
}

TEST(ResultWriter, NoEventWithoutStatusChange) {
    World world;
    auto writer = world.StartWriter();

    writer.Process(Up());
    writer.Process(Down()); // одна неудача — ещё не падение
    writer.Process(Up());

    EXPECT_TRUE(world.publisher.events.empty());
    EXPECT_EQ(world.store.results.size(), 3u);
}

TEST(ResultWriter, RecoveryEventFollowsDownEvent) {
    World world;
    auto writer = world.StartWriter();

    world.MakeDown(writer);
    writer.Process(Up());

    ASSERT_EQ(world.publisher.events.size(), 2u);
    EXPECT_FALSE(world.publisher.events[0].is_up);
    EXPECT_TRUE(world.publisher.events[1].is_up);
    EXPECT_TRUE(world.store.statuses.at(kUrl).notified_is_up);
}

TEST(ResultWriter, RunProcessesQueueUntilClosed) {
    World world;
    auto writer = world.StartWriter();
    BoundedThreadSafeQueue<CheckerResult> queue(4);
    ASSERT_TRUE(queue.push(Down()));
    ASSERT_TRUE(queue.push(Down()));
    queue.close();

    writer.Run(queue);

    EXPECT_EQ(world.store.results.size(), 2u);
    EXPECT_EQ(world.publisher.events.size(), 1u);
}

// ---- 8.2: падения и досылка ----

// Падение до коммита: в «БД» ничего, событий нет, после рестарта досылать нечего.
TEST(ResultWriter, CrashBeforeCommitLeavesNoTrace) {
    World world;
    {
        auto writer = world.StartWriter();
        writer.Process(Down());
        world.store.crash_on_save = true;
        EXPECT_THROW(writer.Process(Down()), SimulatedCrash);
    }
    EXPECT_TRUE(world.store.statuses.at(kUrl).is_up);

    world.StartWriter();
    EXPECT_TRUE(world.publisher.events.empty());
}

// Падение между коммитом и отправкой: после рестарта событие досылается до новых проверок.
TEST(ResultWriter, CrashBeforePublishIsResentOnRecover) {
    World world;
    {
        auto writer = world.StartWriter();
        writer.Process(Down());
        world.publisher.crash_before_send = true;
        EXPECT_THROW(writer.Process(Down()), SimulatedCrash);
    }
    EXPECT_FALSE(world.store.statuses.at(kUrl).is_up);
    EXPECT_TRUE(world.store.statuses.at(kUrl).notified_is_up);
    EXPECT_TRUE(world.publisher.events.empty());

    world.StartWriter();

    ASSERT_EQ(world.publisher.events.size(), 1u);
    EXPECT_FALSE(world.publisher.events[0].is_up);
    EXPECT_EQ(world.publisher.events[0].http_code, 503);
    EXPECT_FALSE(world.store.statuses.at(kUrl).notified_is_up);
}

// Падение после подтверждения брокера, но до отметки: дубль, но не потеря.
TEST(ResultWriter, CrashBeforeMarkGivesDuplicateNotLoss) {
    World world;
    {
        auto writer = world.StartWriter();
        writer.Process(Down());
        world.store.crash_on_mark = true;
        EXPECT_THROW(writer.Process(Down()), SimulatedCrash);
    }
    ASSERT_EQ(world.publisher.events.size(), 1u);

    world.StartWriter();

    ASSERT_EQ(world.publisher.events.size(), 2u);
    EXPECT_EQ(world.publisher.events[0], world.publisher.events[1]);
    EXPECT_FALSE(world.store.statuses.at(kUrl).notified_is_up);
}

// Брокер не подтвердил: расхождение остаётся и уходит со следующим результатом сайта.
TEST(ResultWriter, UnackedEventIsRetriedWithNextResult) {
    World world;
    auto writer = world.StartWriter();
    world.publisher.ack = false;
    world.MakeDown(writer);
    EXPECT_TRUE(world.publisher.events.empty());
    EXPECT_TRUE(world.store.statuses.at(kUrl).notified_is_up);

    world.publisher.ack = true;
    writer.Process(Down()); // сайт всё ещё лежит

    ASSERT_EQ(world.publisher.events.size(), 1u);
    EXPECT_FALSE(world.publisher.events[0].is_up);
    EXPECT_FALSE(world.store.statuses.at(kUrl).notified_is_up);
}

// Сайт восстановился, пока чекер лежал: сначала досылается «упал», потом приходит «восстановился».
TEST(ResultWriter, SiteRecoveredWhileCheckerWasDown) {
    World world;
    {
        auto writer = world.StartWriter();
        writer.Process(Down());
        world.publisher.crash_before_send = true;
        EXPECT_THROW(writer.Process(Down()), SimulatedCrash);
    }

    auto writer = world.StartWriter();
    writer.Process(Up());

    ASSERT_EQ(world.publisher.events.size(), 2u);
    EXPECT_FALSE(world.publisher.events[0].is_up);
    EXPECT_TRUE(world.publisher.events[1].is_up);
    EXPECT_TRUE(world.store.statuses.at(kUrl).notified_is_up);
}

// Статус сохранён и подтверждён до рестарта: досылать нечего, повторного «упал» нет.
TEST(ResultWriter, ConsistentStateSendsNothingOnRecover) {
    World world;
    {
        auto writer = world.StartWriter();
        world.MakeDown(writer);
    }
    ASSERT_EQ(world.publisher.events.size(), 1u);

    auto writer = world.StartWriter();
    writer.Process(Down());

    EXPECT_EQ(world.publisher.events.size(), 1u);
}
