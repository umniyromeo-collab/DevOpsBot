#include "commands.h"

#include "fake_bot_store.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using telegram::CommandHandler;
using telegram::ParseSubscribe;
using telegram::testing::FakeBotStore;

namespace {

    const std::string kA = "https://a.example";
    const std::string kB = "https://b.example";
    const std::string kC = "https://c.example";
    constexpr std::int64_t kChat = 42;

    FakeBotStore StoreWithSites() {
        FakeBotStore store;
        store.sites = {kA, kB, kC};
        return store;
    }

    bool Contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

} // namespace

// ---- разбор команды ----

TEST(ParseSubscribe, SplitsUrlsAndDropsDuplicates) {
    const auto command = ParseSubscribe("/subscribe " + kA + ", " + kB + "\n" + kA);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->urls, (std::vector<std::string>{kA, kB}));
}

TEST(ParseSubscribe, AcceptsBotNameSuffixFromGroups) {
    const auto command = ParseSubscribe("  /subscribe@DevOpsBot " + kA);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->urls, std::vector<std::string>{kA});
}

TEST(ParseSubscribe, EmptyCommandMeansNoUrls) {
    const auto command = ParseSubscribe("/subscribe");
    ASSERT_TRUE(command.has_value());
    EXPECT_TRUE(command->urls.empty());
}

TEST(ParseSubscribe, OtherTextIsNotACommand) {
    EXPECT_FALSE(ParseSubscribe("привет").has_value());
    EXPECT_FALSE(ParseSubscribe("/subscribers " + kA).has_value());
    EXPECT_FALSE(ParseSubscribe("/start").has_value());
    EXPECT_FALSE(ParseSubscribe("").has_value());
}

// ---- сценарии из specs/site-subscriptions ----

TEST(CommandHandler, FirstSubscription) {
    auto store = StoreWithSites();
    CommandHandler handler(store);

    const std::string reply = handler.Handle(kChat, "/subscribe " + kA + " " + kB);

    EXPECT_EQ(store.subscriptions[kChat], (std::vector<std::string>{kA, kB}));
    EXPECT_TRUE(Contains(reply, kA));
    EXPECT_TRUE(Contains(reply, kB));
}

TEST(CommandHandler, NewListReplacesOldOne) {
    auto store = StoreWithSites();
    store.subscriptions[kChat] = {kA, kB};
    CommandHandler handler(store);

    (void)handler.Handle(kChat, "/subscribe " + kC);

    EXPECT_EQ(store.subscriptions[kChat], std::vector<std::string>{kC});
}

TEST(CommandHandler, CommandWithoutUrlsRemovesAll) {
    auto store = StoreWithSites();
    store.subscriptions[kChat] = {kA};
    CommandHandler handler(store);

    const std::string reply = handler.Handle(kChat, "/subscribe");

    EXPECT_TRUE(store.subscriptions[kChat].empty());
    EXPECT_TRUE(Contains(reply, "сняты"));
}

TEST(CommandHandler, UnknownUrlIsRejectedKnownIsSaved) {
    auto store = StoreWithSites();
    CommandHandler handler(store);

    const std::string reply = handler.Handle(kChat, "/subscribe " + kA + " https://typo.example");

    EXPECT_EQ(store.subscriptions[kChat], std::vector<std::string>{kA});
    EXPECT_TRUE(Contains(reply, "https://typo.example"));
}

TEST(CommandHandler, AllUnknownLeavesSubscriptionsUnchanged) {
    auto store = StoreWithSites();
    store.subscriptions[kChat] = {kA};
    CommandHandler handler(store);

    const std::string reply =
        handler.Handle(kChat, "/subscribe https://x.example https://y.example");

    EXPECT_EQ(store.subscriptions[kChat], std::vector<std::string>{kA});
    EXPECT_EQ(store.replace_calls, 0);
    EXPECT_TRUE(Contains(reply, "https://x.example"));
    EXPECT_TRUE(Contains(reply, "не изменены"));
}

TEST(CommandHandler, OtherTextGetsHelpAndChangesNothing) {
    auto store = StoreWithSites();
    store.subscriptions[kChat] = {kA};
    CommandHandler handler(store);

    const std::string reply = handler.Handle(kChat, "что ты умеешь?");

    EXPECT_EQ(reply, telegram::HelpText());
    EXPECT_EQ(store.replace_calls, 0);
}

// ---- текст уведомления ----

TEST(StatusMessage, DownMentionsUrlAndCode) {
    const std::string text = telegram::StatusMessage({kA, false, 503, 0});
    EXPECT_TRUE(Contains(text, "упал"));
    EXPECT_TRUE(Contains(text, kA));
    EXPECT_TRUE(Contains(text, "503"));
    EXPECT_TRUE(Contains(text, "1970-01-01 00:00:00 UTC"));
}

TEST(StatusMessage, NoResponseIsSpelledOut) {
    EXPECT_TRUE(Contains(telegram::StatusMessage({kA, false, 0, 0}), "нет ответа"));
}

TEST(StatusMessage, RecoveryMentionsUrl) {
    const std::string text = telegram::StatusMessage({kA, true, 200, 0});
    EXPECT_TRUE(Contains(text, "снова работает"));
    EXPECT_TRUE(Contains(text, kA));
}
