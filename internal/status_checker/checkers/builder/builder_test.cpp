#include "../Ifetcher.h"
#include "builder.h"

#include <gtest/gtest.h>

#include <string>

namespace {

    class MockFetcher : public net::IFetcher {
    public:
        int code_to_return = 200;
        mutable std::string last_requested_url;

        int FetchStatusCode(const std::string &url) const override {
            last_requested_url = url;
            return code_to_return;
        }
    };

} // namespace

TEST(Builder, HttpUrlGivesHttpConfig) {
    const auto config = Builder::ConfigFromSite("http://example.com", 204);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->type, Builder::ConfigType::http);
    const auto &http = std::get<HTTPChecker::Config>(config->config);
    EXPECT_EQ(http.url, "http://example.com");
    EXPECT_EQ(http.http_code, 204);
}

TEST(Builder, HttpsUrlGivesHttpsConfig) {
    const auto config = Builder::ConfigFromSite("https://example.com", 200);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->type, Builder::ConfigType::https);
    EXPECT_EQ(std::get<HTTPSChecker::Config>(config->config).url, "https://example.com");
}

// Неизвестная схема отсеивается здесь, поэтому до CreateChecker она не доходит.
TEST(Builder, UnsupportedSchemeIsRejected) {
    EXPECT_FALSE(Builder::ConfigFromSite("ftp://example.com", 200).has_value());
    EXPECT_FALSE(Builder::ConfigFromSite("example.com", 200).has_value());
    EXPECT_FALSE(Builder::ConfigFromSite("", 200).has_value());
}

// Для любого конфига из ConfigFromSite чекер создаётся и ходит в сеть через переданный фетчер.
TEST(Builder, EverySupportedConfigCreatesWorkingChecker) {
    MockFetcher fetcher;
    for (const std::string url : {"http://a.example", "https://b.example"}) {
        const auto config = Builder::ConfigFromSite(url, 200);
        ASSERT_TRUE(config.has_value()) << url;

        const auto checker = Builder::CreateChecker(*config, fetcher);
        ASSERT_NE(checker, nullptr) << url;
        EXPECT_EQ(checker->URL(), url);

        const CheckerResult result = checker->Check();
        EXPECT_TRUE(result.is_up) << url;
        EXPECT_EQ(fetcher.last_requested_url, url);
    }
}
