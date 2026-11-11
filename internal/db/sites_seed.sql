-- Фиксированный список сайтов для проверки. Это пример: замените адреса на свои.
-- Повторное применение ничего не меняет (ON CONFLICT DO NOTHING); чтобы поменять интервал
-- у существующего сайта, используйте UPDATE. Чекер читает список только при старте.
INSERT INTO sites (url, base_interval_sec, expected_http_code)
VALUES ('https://example.com', 60, 200),
       ('https://www.google.com', 60, 200),
       ('https://github.com', 120, 200)
ON CONFLICT (url) DO NOTHING;
