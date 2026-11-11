CREATE TABLE IF NOT EXISTS check_results (
    id         BIGSERIAL PRIMARY KEY,
    url        TEXT        NOT NULL,
    http_code  INT         NOT NULL,
    is_up      BOOLEAN     NOT NULL,
    checked_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Таблицу могла создать ранняя версия схемы без http_code: CREATE TABLE IF NOT EXISTS её
-- пропустит, поэтому колонку добавляем отдельно. У старых строк код неизвестен — 0.
ALTER TABLE check_results ADD COLUMN IF NOT EXISTS http_code INT NOT NULL DEFAULT 0;

CREATE INDEX IF NOT EXISTS check_results_url_time_idx
    ON check_results (url, checked_at DESC);

-- Фиксированный список сайтов. Чекер читает его один раз при старте.
CREATE TABLE IF NOT EXISTS sites (
    url                TEXT PRIMARY KEY,
    base_interval_sec  INT  NOT NULL CHECK (base_interval_sec > 0),
    expected_http_code INT  NOT NULL DEFAULT 200
);

-- Статус сайта. is_up — что есть на самом деле, notified_is_up — о чём Kafka уже подтвердила
-- приём. Пока они различаются, чекер досылает событие: так смена статуса не теряется при падении.
CREATE TABLE IF NOT EXISTS site_state (
    url                  TEXT        PRIMARY KEY REFERENCES sites (url),
    is_up                BOOLEAN     NOT NULL,
    notified_is_up       BOOLEAN     NOT NULL,
    consecutive_failures INT         NOT NULL DEFAULT 0,
    changed_at           TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Подписки пользователей Telegram: на какие сайты из списка приходят уведомления.
CREATE TABLE IF NOT EXISTS subscriptions (
    chat_id BIGINT NOT NULL,
    url     TEXT   NOT NULL REFERENCES sites (url),
    PRIMARY KEY (chat_id, url)
);

CREATE INDEX IF NOT EXISTS subscriptions_url_idx
    ON subscriptions (url);