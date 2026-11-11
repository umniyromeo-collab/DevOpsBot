#pragma once

#include "../Ifetcher.h"
#include "../checker.h"
#include "config.h"

#include <memory>
#include <optional>
#include <string>

namespace Builder{
    // Конфиг проверки для сайта из списка: тип выбирается по схеме URL.
    // nullopt — схема не поддерживается (не http:// и не https://): такой сайт проверять нечем,
    // и его нужно отсеять до создания чекера, а не получить nullptr из CreateChecker.
    [[nodiscard]] std::optional<Config> ConfigFromSite(const std::string &url,
                                                       int expected_http_code);

    // fetcher должен жить дольше созданного чекера: чекер хранит на него ссылку.
    std::unique_ptr<IChecker> CreateChecker(const Config &url, net::IFetcher &fetcher);
} // namespace Builder
