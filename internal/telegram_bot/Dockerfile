# Многостадийная сборка: тяжёлый тулчейн остаётся в builder,
# в финальный образ уезжает только сам бинарник.
#
# Собирать из корня репозитория — CMakeLists.txt лежит там:
#   docker build -f internal/telegram_bot/Dockerfile -t devopsbot-telegram .

# ---------- стадия 1: сборка ----------
FROM debian:bookworm-slim AS builder

# git нужен FetchContent'у: cpr, nlohmann/json и googletest тянутся из GitHub.
# libcurl4-openssl-dev — потому что CMakeLists ставит CPR_USE_SYSTEM_CURL=ON.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        libcurl4-openssl-dev \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY ../../CMakeLists.txt ./
COPY .. ./internal

# Собираем только цель TelegramBot: DevOpsBot и checker_test для деплоя бота
# не нужны (и checker_test сейчас красный — пусть не ломает сборку образа).
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --target TelegramBot -j 2

# ---------- стадия 2: рантайм ----------
FROM debian:bookworm-slim AS runtime

# libcurl4 — cpr слинкован с системным curl динамически.
# ca-certificates — без корневых сертификатов не поднимется TLS к api.telegram.org.
# tzdata — иначе localtime_r() умеет только UTC, а бот именно время и отдаёт.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libcurl4 \
        libstdc++6 \
        ca-certificates \
        tzdata \
    && rm -rf /var/lib/apt/lists/*

# Не работаем root'ом: боту не нужны никакие права в системе.
RUN useradd --create-home --shell /usr/sbin/nologin bot

COPY --from=builder /src/build/TelegramBot /usr/local/bin/TelegramBot

USER bot
WORKDIR /home/bot

# Переопределяется через `docker run -e TZ=Europe/Moscow`.
ENV TZ=UTC

# Токен НЕ вшиваем в образ — только через окружение на запуске:
#   docker run -e TELEGRAM_BOT_TOKEN=... devopsbot-telegram
# Бот сам завершится с кодом 1, если переменная не задана.
ENTRYPOINT ["/usr/local/bin/TelegramBot"]
