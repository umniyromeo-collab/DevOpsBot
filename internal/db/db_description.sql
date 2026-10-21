CREATE TABLE IF NOT EXISTS check_results (
    id         BIGSERIAL PRIMARY KEY,
    url        TEXT        NOT NULL,
    http_code  INT         NOT NULL,
    is_up      BOOLEAN     NOT NULL,
    checked_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS check_results_url_time_idx
    ON check_results (url, checked_at DESC);