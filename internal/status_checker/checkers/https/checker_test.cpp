#include "../Ifetcher.h" // net::IFetcher
#include "checker.h"     // HTTPSChecker::Checker
#include "config.h"      // HTTPSChecker::Config

#include <gtest/gtest.h>

#include <string>

namespace {

    // Фейковый фетчер: в сеть не ходит, возвращает заранее заданный код
    // и запоминает, какой URL у него спросили.
    class MockFetcher : public net::IFetcher {
    public:
        int code_to_return = 0;
        mutable std::string last_requested_url;

        int FetchStatusCode(const std::string &url) const override {
            last_requested_url = url;
            return code_to_return;
        }
    };

} // namespace

TEST(HttpsChecker, IsUpWhenCodeMatchesExpected) {
    MockFetcher fetcher;
    fetcher.code_to_return = 200;
    HTTPSChecker::Checker checker(HTTPSChecker::Config{"https://example.com", 200}, fetcher);

    const CheckerResult result = checker.Check();
    EXPECT_TRUE(result.is_up);
    EXPECT_EQ(result.http_code, 200);
}

TEST(HttpsChecker, IsDownWhenCodeDiffers) {
    MockFetcher fetcher;
    fetcher.code_to_return = 503;
    HTTPSChecker::Checker checker(HTTPSChecker::Config{"https://example.com", 200}, fetcher);

    const CheckerResult result = checker.Check();
    EXPECT_FALSE(result.is_up);
    EXPECT_EQ(result.http_code, 503);
}

// Ответа не было вовсе (код 0) — это неудачная проверка.
TEST(HttpsChecker, IsDownWhenUnreachable) {
    MockFetcher fetcher;
    fetcher.code_to_return = 0;
    HTTPSChecker::Checker checker(HTTPSChecker::Config{"https://example.com", 200}, fetcher);

    const CheckerResult result = checker.Check();
    EXPECT_FALSE(result.is_up);
    EXPECT_EQ(result.http_code, 0);
}

TEST(HttpsChecker, RequestsConfiguredUrl) {
    MockFetcher fetcher;
    fetcher.code_to_return = 200;
    HTTPSChecker::Checker checker(HTTPSChecker::Config{"https://my-site.org/health", 200}, fetcher);

    (void)checker.Check();

    EXPECT_EQ(fetcher.last_requested_url, "https://my-site.org/health");
}

TEST(HttpsChecker, UrlReturnsConfiguredAddress) {
    MockFetcher fetcher;
    HTTPSChecker::Checker checker(HTTPSChecker::Config{"https://my-site.org", 200}, fetcher);

    EXPECT_EQ(checker.URL(), "https://my-site.org");
}
