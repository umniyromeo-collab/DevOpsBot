#include "notifier.h"

#include "fake_bot_store.h"

#include <gtest/gtest.h>

#include <atomic>
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using telegram::Notifier;
using telegram::SendOutcome;
using telegram::SendResult;
using telegram::testing::FakeBotStore;

namespace {

    const std::string kUrl = "https://a.example";

    // Источник событий: отдаёт заданные события, а когда они кончились — взводит stop,
    // чтобы Run завершился. Пишет коммиты в общий журнал.
    class FakeSource final : public kafka::IEventSource {
    public:
        FakeSource(std::atomic<bool> &stop, std::vector<std::string> &journal)
            : stop_(stop), journal_(journal) {}

        std::deque<kafka::SiteStatusEvent> events;
        int commits = 0;

        std::optional<kafka::SiteStatusEvent> Next(std::chrono::milliseconds) override {
            if (events.empty()) {
                stop_ = true;
                return std::nullopt;
            }
            auto event = events.front();
            events.pop_front();
            return event;
        }

        void Commit() override {
            ++commits;
            journal_.push_back("commit");
        }

    private:
        std::atomic<bool> &stop_;
        std::vector<std::string> &journal_;
    };

    // Отправка по сценарию: ответы для чата берутся по очереди, последний повторяется.
    class FakeSender final : public telegram::IMessageSender {
    public:
        explicit FakeSender(std::vector<std::string> &journal) : journal_(journal) {}

        std::map<std::int64_t, std::vector<SendResult>> script;
        std::map<std::int64_t, int> attempts;

        SendResult Send(std::int64_t chat_id, const std::string &text) override {
            journal_.push_back("send " + std::to_string(chat_id) + " " +
                               (text.find("упал") != std::string::npos ? "down" : "up"));
            auto &answers = script[chat_id];
            const int attempt = attempts[chat_id]++;
            if (answers.empty()) {
                return {SendOutcome::Sent};
            }
            return answers[std::min<std::size_t>(attempt, answers.size() - 1)];
        }

    private:
        std::vector<std::string> &journal_;
    };

    struct World {
        std::atomic<bool> stop{false};
        std::vector<std::string> journal;
        std::vector<std::chrono::milliseconds> sleeps;
        FakeSource source{stop, journal};
        FakeBotStore store;
        FakeSender sender{journal};
        Notifier notifier{source, store, sender, stop,
                          [this](std::chrono::milliseconds pause) { sleeps.push_back(pause); }};
    };

} // namespace

TEST(Notifier, CommitsAfterEveryRecipient) {
    World world;
    world.store.subscriptions = {{1, {kUrl}}, {2, {kUrl}}, {3, {"https://other.example"}}};
    world.source.events = {{kUrl, false, 503, 0}};

    world.notifier.Run();

    const std::vector<std::string> expected{"send 1 down", "send 2 down", "commit"};
    EXPECT_EQ(world.journal, expected);
}

TEST(Notifier, EventsAreDeliveredInTopicOrder) {
    World world;
    world.store.subscriptions = {{1, {kUrl}}};
    world.source.events = {{kUrl, false, 503, 0}, {kUrl, true, 200, 60}};

    world.notifier.Run();

    const std::vector<std::string> expected{"send 1 down", "commit", "send 1 up", "commit"};
    EXPECT_EQ(world.journal, expected);
}

TEST(Notifier, TemporaryErrorIsRetriedBeforeCommit) {
    World world;
    world.store.subscriptions = {{1, {kUrl}}};
    world.sender.script[1] = {{SendOutcome::Retry, 7s}, {SendOutcome::Sent}};
    world.source.events = {{kUrl, false, 0, 0}};

    world.notifier.Run();

    EXPECT_EQ(world.sender.attempts[1], 2);
    EXPECT_EQ(world.source.commits, 1);
    ASSERT_EQ(world.sleeps.size(), 1u);
    EXPECT_EQ(world.sleeps[0], 7s) << "пауза, которую попросил Telegram";
}

// specs/site-status-notifications: «Недоступный получатель не блокирует доставку».
TEST(Notifier, RejectedRecipientIsSkipped) {
    World world;
    world.store.subscriptions = {{1, {kUrl}}, {2, {kUrl}}};
    world.sender.script[1] = {{SendOutcome::Skip}};
    world.source.events = {{kUrl, false, 503, 0}};

    world.notifier.Run();

    EXPECT_EQ(world.sender.attempts[1], 1);
    EXPECT_EQ(world.sender.attempts[2], 1);
    EXPECT_EQ(world.source.commits, 1);
}

// Остановка посреди повторов: offset не коммитится, событие придёт снова после перезапуска.
TEST(Notifier, StopDuringRetryLeavesEventUncommitted) {
    World world;
    world.store.subscriptions = {{1, {kUrl}}};
    world.sender.script[1] = {{SendOutcome::Retry}};
    world.source.events = {{kUrl, false, 503, 0}};
    Notifier notifier{world.source, world.store, world.sender, world.stop,
                      [&world](std::chrono::milliseconds) { world.stop = true; }};

    notifier.Run();

    EXPECT_EQ(world.source.commits, 0);
}

TEST(Notifier, EventWithoutSubscribersIsCommitted) {
    World world;
    world.source.events = {{kUrl, true, 200, 0}};

    world.notifier.Run();

    EXPECT_EQ(world.source.commits, 1);
    EXPECT_TRUE(world.sender.attempts.empty());
}

TEST(Notifier, BackoffGrowsWithoutRetryAfter) {
    World world;
    world.store.subscriptions = {{1, {kUrl}}};
    world.sender.script[1] = {{SendOutcome::Retry}, {SendOutcome::Retry}, {SendOutcome::Sent}};

    EXPECT_TRUE(world.notifier.Deliver({kUrl, false, 503, 0}));

    EXPECT_EQ(world.sleeps, (std::vector<std::chrono::milliseconds>{1s, 2s}));
}
