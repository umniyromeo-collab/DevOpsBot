#include "checker.h"      // HTTPChecker::Checker
#include "config.h"       // HTTPChecker::Config
#include "../fetcher.h"   // net::IFetcher

#include <gtest/gtest.h>

#include <string>

namespace {

// Фейковый фетчер: в сеть не ходит, возвращает заранее заданный код
// и запоминает, какой URL у него спросили. Это и есть подмена "шва".
class FakeFetcher : public net::IFetcher {
public:
    int code_to_return = 0;
    mutable std::string last_requested_url;

    int FetchStatusCode(const std::string& url) const override {
        last_requested_url = url;
        return code_to_return;
    }
};

}  // namespace

// Сервис ответил ожидаемым кодом -> сервис жив -> Check() == true.
TEST(HttpChecker, IsUpWhenCodeMatchesExpected) {
    FakeFetcher fetcher;
    fetcher.code_to_return = 200;
    HTTPChecker::Checker checker(HTTPChecker::Config{"http://example.com", 200}, fetcher);

    EXPECT_TRUE(checker.Check());
}

// Хост ответил, но не тем кодом, что ждали -> Check() == false.
TEST(HttpChecker, IsDownWhenCodeDiffers) {
    FakeFetcher fetcher;
    fetcher.code_to_return = 404;
    HTTPChecker::Checker checker(HTTPChecker::Config{"http://example.com", 200}, fetcher);

    EXPECT_FALSE(checker.Check());
}

// Ответа не было вовсе (код 0) -> Check() == false.
TEST(HttpChecker, IsDownWhenUnreachable) {
    FakeFetcher fetcher;
    fetcher.code_to_return = 0;
    HTTPChecker::Checker checker(HTTPChecker::Config{"http://example.com", 200}, fetcher);

    EXPECT_FALSE(checker.Check());
}

// Checker обязан спрашивать ровно тот URL, что задан в конфиге.
TEST(HttpChecker, RequestsConfiguredUrl) {
    FakeFetcher fetcher;
    fetcher.code_to_return = 200;
    HTTPChecker::Checker checker(HTTPChecker::Config{"http://my-site.org/health", 200}, fetcher);

    checker.Check();

    EXPECT_EQ(fetcher.last_requested_url, "http://my-site.org/health");
}

// URL() возвращает адрес из конфига (метод уже есть — тест-страховка от регрессий).
TEST(HttpChecker, UrlReturnsConfiguredAddress) {
    FakeFetcher fetcher;
    HTTPChecker::Checker checker(HTTPChecker::Config{"http://my-site.org", 200}, fetcher);

    EXPECT_EQ(checker.URL(), "http://my-site.org");
}
