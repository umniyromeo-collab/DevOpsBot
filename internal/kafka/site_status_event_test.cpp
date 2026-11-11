#include "site_status_event.h"

#include <gtest/gtest.h>

using kafka::FromJson;
using kafka::SiteStatusEvent;
using kafka::ToJson;

TEST(SiteStatusEvent, RoundTrip) {
    const SiteStatusEvent down{"https://example.com/health", false, 503, 1731000000};
    const SiteStatusEvent up{"http://127.0.0.1:8765/", true, 200, 1731000060};

    EXPECT_EQ(FromJson(ToJson(down)), down);
    EXPECT_EQ(FromJson(ToJson(up)), up);
}

TEST(SiteStatusEvent, RejectsGarbage) {
    EXPECT_FALSE(FromJson("").has_value());
    EXPECT_FALSE(FromJson("not json").has_value());
    EXPECT_FALSE(FromJson("[1, 2, 3]").has_value());
}

TEST(SiteStatusEvent, RejectsMissingField) {
    EXPECT_FALSE(FromJson(R"({"url": "https://a", "is_up": true, "http_code": 200})").has_value());
}

TEST(SiteStatusEvent, RejectsWrongType) {
    EXPECT_FALSE(
        FromJson(R"({"url": "https://a", "is_up": "yes", "http_code": 200, "changed_at": 1})")
            .has_value());
}
