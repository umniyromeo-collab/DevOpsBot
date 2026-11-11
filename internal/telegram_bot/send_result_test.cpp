#include "send_result.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using telegram::ClassifySendResponse;
using telegram::SendOutcome;

TEST(ClassifySendResponse, OkIsSent) {
    EXPECT_EQ(ClassifySendResponse(200, R"({"ok":true})").outcome, SendOutcome::Sent);
}

TEST(ClassifySendResponse, NoResponseIsRetried) {
    EXPECT_EQ(ClassifySendResponse(0, "").outcome, SendOutcome::Retry);
}

TEST(ClassifySendResponse, TooManyRequestsHonoursRetryAfter) {
    const auto result = ClassifySendResponse(
        429, R"({"ok":false,"error_code":429,"parameters":{"retry_after":7}})");
    EXPECT_EQ(result.outcome, SendOutcome::Retry);
    EXPECT_EQ(result.retry_after, 7s);
}

TEST(ClassifySendResponse, TooManyRequestsWithoutHintStillRetries) {
    const auto result = ClassifySendResponse(429, "not json");
    EXPECT_EQ(result.outcome, SendOutcome::Retry);
    EXPECT_EQ(result.retry_after, 0s);
}

TEST(ClassifySendResponse, ServerErrorIsRetried) {
    EXPECT_EQ(ClassifySendResponse(502, "").outcome, SendOutcome::Retry);
}

// Пользователь заблокировал бота или чата нет — повторять бессмысленно.
TEST(ClassifySendResponse, ClientErrorSkipsRecipient) {
    EXPECT_EQ(ClassifySendResponse(403, R"({"ok":false})").outcome, SendOutcome::Skip);
    EXPECT_EQ(ClassifySendResponse(400, R"({"ok":false})").outcome, SendOutcome::Skip);
}
