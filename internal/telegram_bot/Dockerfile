FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        libcurl4-openssl-dev \
        libssl-dev \
        libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt ./
COPY internal ./internal

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DADDRESS_SANITIZER_AVAILABLE=OFF \
        -DTHREAD_SANITIZER_AVAILABLE=OFF \
    && cmake --build build --target TelegramBot -j 2

FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libcurl4 \
        libstdc++6 \
        ca-certificates \
        tzdata \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --shell /usr/sbin/nologin bot

COPY --from=builder /src/build/TelegramBot /usr/local/bin/TelegramBot

USER bot
WORKDIR /home/bot

ENV TZ=UTC

ENTRYPOINT ["/usr/local/bin/TelegramBot"]