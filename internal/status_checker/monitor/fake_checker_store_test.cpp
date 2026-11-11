#include "fake_checker_store.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using monitor::testing::FakeCheckerStore;

// Фейк должен вести себя как PG::CheckerStore, иначе тесты логики проверяют не то.
TEST(FakeCheckerStore, LoadStatusesCreatesMissingRowsAsUp) {
    FakeCheckerStore store;
    store.sites = {{"https://a.example", 60s, 200}};

    const auto statuses = store.LoadStatuses();
    ASSERT_EQ(statuses.size(), 1u);
    EXPECT_TRUE(statuses.at("https://a.example").is_up);
    EXPECT_TRUE(statuses.at("https://a.example").notified_is_up);
}

TEST(FakeCheckerStore, SaveResultKeepsNotifiedStatus) {
    FakeCheckerStore store;
    store.sites = {{"https://a.example", 60s, 200}};
    (void)store.LoadStatuses();

    monitor::SiteStatus down;
    down.is_up = false;
    down.notified_is_up = false; // SaveResult это поле не трогает
    store.SaveResult({"https://a.example", 0, false}, down);

    EXPECT_FALSE(store.statuses.at("https://a.example").is_up);
    EXPECT_TRUE(store.statuses.at("https://a.example").notified_is_up);
    EXPECT_EQ(store.results.size(), 1u);
}
