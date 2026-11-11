#include "status_tracker.h"

#include <gtest/gtest.h>

using monitor::ApplyCheck;
using monitor::SiteStatus;

namespace {

    SiteStatus Down() {
        SiteStatus status;
        status.is_up = false;
        status.notified_is_up = false;
        status.consecutive_failures = monitor::FAILURES_TO_DOWN;
        return status;
    }

} // namespace

// Сценарии — из specs/site-monitoring: «Определение падения сайта».
TEST(StatusTracker, SingleFailureKeepsSiteUp) {
    const auto update = ApplyCheck(SiteStatus{}, false, 503);
    EXPECT_FALSE(update.changed);
    EXPECT_TRUE(update.status.is_up);
    EXPECT_EQ(update.status.consecutive_failures, 1);
}

TEST(StatusTracker, TwoFailuresInARowMarkSiteDown) {
    const auto first = ApplyCheck(SiteStatus{}, false, 503);
    const auto second = ApplyCheck(first.status, false, 0);
    EXPECT_TRUE(second.changed);
    EXPECT_FALSE(second.status.is_up);
    EXPECT_EQ(second.status.consecutive_failures, 2);
}

TEST(StatusTracker, SuccessBetweenFailuresResetsCounter) {
    auto update = ApplyCheck(SiteStatus{}, false, 503);
    update = ApplyCheck(update.status, true, 200);
    EXPECT_EQ(update.status.consecutive_failures, 0);
    update = ApplyCheck(update.status, false, 503);
    EXPECT_FALSE(update.changed);
    EXPECT_TRUE(update.status.is_up);
}

// «Определение восстановления сайта».
TEST(StatusTracker, FirstSuccessRecoversDownSite) {
    const auto update = ApplyCheck(Down(), true, 200);
    EXPECT_TRUE(update.changed);
    EXPECT_TRUE(update.status.is_up);
    EXPECT_EQ(update.status.consecutive_failures, 0);
}

// «Начальный статус сайта».
TEST(StatusTracker, NewSiteThatWorksIsNotAChange) {
    const auto update = ApplyCheck(SiteStatus{}, true, 200);
    EXPECT_FALSE(update.changed);
    EXPECT_TRUE(update.status.is_up);
}

TEST(StatusTracker, NewSiteThatIsDownGoesDownAtThreshold) {
    SiteStatus status;
    bool changed = false;
    for (int i = 0; i < monitor::FAILURES_TO_DOWN; ++i) {
        const auto update = ApplyCheck(status, false, 0);
        status = update.status;
        changed = update.changed;
    }
    EXPECT_TRUE(changed);
    EXPECT_FALSE(status.is_up);
}

// «Сообщения только при смене статуса»: лежащий сайт продолжает лежать — смены нет.
TEST(StatusTracker, DownSiteStayingDownIsNotAChange) {
    const auto update = ApplyCheck(Down(), false, 0);
    EXPECT_FALSE(update.changed);
    EXPECT_FALSE(update.status.is_up);
    EXPECT_EQ(update.status.consecutive_failures, monitor::FAILURES_TO_DOWN + 1);
}

// notified_is_up меняется только после подтверждения Kafka, а не здесь.
TEST(StatusTracker, DoesNotTouchNotifiedStatusOrTimestamp) {
    SiteStatus status;
    status.changed_at = 1234;
    auto update = ApplyCheck(status, false, 0);
    update = ApplyCheck(update.status, false, 0);
    ASSERT_TRUE(update.changed);
    EXPECT_TRUE(update.status.notified_is_up);
    EXPECT_EQ(update.status.changed_at, 1234);
}

TEST(StatusTracker, RemembersLastHttpCode) {
    EXPECT_EQ(ApplyCheck(SiteStatus{}, false, 502).status.last_http_code, 502);
    EXPECT_EQ(ApplyCheck(SiteStatus{}, true, 200).status.last_http_code, 200);
}
