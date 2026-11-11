#include "priority.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using monitor::EffectiveInterval;

// Сценарии — из specs/site-monitoring: «Базовая частота» и «Учащённый опрос после сбоев».
TEST(Priority, StableSiteUsesBaseInterval) {
    EXPECT_EQ(EffectiveInterval(60s, 0), 60s);
}

TEST(Priority, RecentFailureSpeedsUpPolling) {
    EXPECT_EQ(EffectiveInterval(60s, 1), 15s);
    EXPECT_EQ(EffectiveInterval(600s, 7), 150s);
}

TEST(Priority, FasterIntervalRespectsFloor) {
    EXPECT_EQ(EffectiveInterval(20s, 3), monitor::MIN_INTERVAL);
}

TEST(Priority, NeverSlowerThanBase) {
    EXPECT_EQ(EffectiveInterval(5s, 1), 5s);
}

TEST(Priority, ReturnsToBaseWhenWindowIsClean) {
    ASSERT_EQ(EffectiveInterval(60s, 2), 15s);
    EXPECT_EQ(EffectiveInterval(60s, 0), 60s);
}
