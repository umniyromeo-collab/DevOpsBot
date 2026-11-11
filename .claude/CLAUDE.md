# DevOpsBot

Учебный проект на C++23: мониторинг доступности сайтов с Telegram-ботом.

- **Чекер** (`DevOpsBot`) в несколько потоков проверяет сайты (сейчас HTTP/HTTPS, дальше — другие
  виды проверок), пишет результаты в PostgreSQL и сам определяет, когда сайт упал или восстановился.
- **Telegram-бот** (`TelegramBot`) принимает от пользователя подписки и присылает подписчикам
  сообщения о падении и восстановлении сайта. События от чекера получает через Kafka.

## Архитектура

Архитектурные контракты — владение данными, контракты потоков, порядок записи и доставки, формат
события, схема — в корневом `CLAUDE.md` (на английском). Это единственный источник: здесь их
не дублируй, а меняя контракт, правь тот файл. Коротко: два процесса (`DevOpsBot` и `TelegramBot`),
между ними Kafka, гарантия доставки at-least-once.

## Текущее состояние (снимок на 2026-11-11)

Сверяйся с кодом; закрыл пункт — обнови раздел. Прогресс изменения — в
`openspec/changes/site-down-notifications/tasks.md`.

- `DevOpsBot` (`internal/status_checker/main.cpp`) — рабочий процесс чекера: планировщик, пул,
  очередь, поток записи в Postgres и Kafka (`internal/status_checker/monitor/`). Проверен на живых
  Postgres и Kafka. Под TSan целиком не запускается: librdkafka создаёт потоки через C11
  `thrd_create`, который TSan не поддерживает (падает и под GCC, и под Clang).
- `HTTPChecker` и `HTTPSChecker` ходят в сеть через `net::IFetcher` и покрыты тестами. Боевой
  `HTTPChecker::Fetcher` на cpr (`http/fetcher.cpp`, таймаут 10 с) входит только в `DevOpsBot`.
  Тип проверки выбирает `Builder::ConfigFromSite` по схеме URL (неизвестная схема — `nullopt`),
  `Builder::CreateChecker` принимает фетчер, который должен пережить чекер.
- `ThreadPool` рабочий: деструктор дорабатывает очередь задач и join-ит потоки.
- `BoundedThreadSafeQueue` рабочая: шаблон целиком в заголовке, все методы под мьютексом,
  `close()` будит ждущих, `push`/`wait_and_pop` возвращают `false` после закрытия (потребитель
  сначала дочитывает остаток). Покрыта тестами, включая стресс и `close()` под нагрузкой; чиста
  под TSan. Безопасность `notify` после `unlock` держится на том, что очередь разрушается после
  join всех, кто её трогает.
- `TelegramBot` — процесс бота: поток команд (`/subscribe`) и поток доставки событий из Kafka.
  Собирается, логика покрыта тестами (`bot_test`), но с настоящим Telegram ещё не проверялся.
- `PG::PGStorage` — универсальный INSERT по `DBEntry`, реализация целиком в `pg_storage.h`
  (`pg_storage.cpp` пустой). Заголовок пока нигде не подключён, ни одна цель его не компилирует.
  `PG::PGStorage s;` не соберётся (`PGStore_t` — const-тип, «uninitialized const member»), `s{}`
  собирается. Одно статическое `pqxx::connection` на процесс, без переподключения: после обрыва
  каждый `Store` бросает исключение. Схема `internal/db/db_description.sql` сама не применяется.
- `PgDemo` не линкуется: в `internal/db/main.cpp` нет `main()`. Поэтому `cmake --build` без
  `--target` падает.
- Docker-образ (`internal/telegram_bot/Dockerfile`) собирает только `TelegramBot`. Когда в образ
  поедет `DevOpsBot` с libpqxx, в runtime-слой понадобится `libpq5`, а базовый образ придётся
  поднять: в `bookworm` GCC 12, а `pg_storage.h` использует `<format>` (есть с GCC 13).
- Вне сборки: `src/`, `cmake/UniquePtr.*`, `cmake/CompilerWarnings.cmake`, пустые
  `internal/status_checker/CMakeLists.txt` и `internal/storage/storage.cpp`,
  `http/curl` (заметка с кодом на libcurl),
  `internal/db/pg_storage.h.gch` — устаревший предкомпилированный заголовок на 127 МБ: Debug-сборка
  его отвергает, но он попадает в docker-контекст (`.dockerignore` пропускает весь `internal/`).

## Карта каталогов

```text
internal/status_checker/main.cpp   точка входа DevOpsBot
internal/status_checker/checkers/  IChecker, CheckerResult, net::IFetcher (шов для подмены сети)
  builder/                         Builder::Config (enum + variant) и фабрика CreateChecker
  http/  https/                    по каталогу на тип: config.h, checker.{h,cpp}, checker_test.cpp
internal/lib/thread_pools/         ThreadPool
internal/lib/thread_queue/         BoundedThreadSafeQueue
internal/telegram_bot/             telegram::Bot (cpr + nlohmann_json), Dockerfile
internal/storage/                  Storage<StoreFunc> — обёртка над функцией записи
internal/db/                       PG::PGStorage (libpqxx), db_description.sql — схема
openspec/                          планирование изменений (OpenSpec)
multithreading.md                  конспект по многопоточности в C++ — справка по примитивам
```

## Сборка, тесты, запуск

Каталог сборки общий для CLion, VS Code и Claude — `cmake-build-debug/`, генератор Unix Makefiles.
Другой генератор сломает его кэш, а отдельный каталог заново соберёт все зависимости (исключение —
`cmake-build-tsan/` для ThreadSanitizer, команды — в `.claude/agents/concurrency-reviewer.md`).

```bash
# конфигурация — если каталога ещё нет; после правки CMakeLists.txt cmake --build перезапустит её сам
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# сборка — всегда с явными целями (PgDemo сейчас не линкуется)
cmake --build cmake-build-debug --target DevOpsBot TelegramBot checker_test bot_test -j

# тесты (GoogleTest + gtest_discover_tests)
ctest --test-dir cmake-build-debug --output-on-failure
./cmake-build-debug/checker_test --gtest_filter='HttpChecker.*'

# Postgres и Kafka из docker-compose, схема (psql и утилиты Kafka есть только внутри контейнеров)
docker compose up -d --wait postgres kafka
docker compose exec -T postgres sh -c 'psql -U "$POSTGRES_USER" -d "$POSTGRES_DB"' < internal/db/db_description.sql
docker compose exec -T postgres sh -c 'psql -U "$POSTGRES_USER" -d "$POSTGRES_DB"' < internal/db/sites_seed.sql

# события в топике site-status (ключ — URL)
docker compose exec -T kafka /opt/kafka/bin/kafka-console-consumer.sh --bootstrap-server localhost:9092 \
  --topic site-status --from-beginning --property print.key=true --timeout-ms 5000

# запуск с переменными из .env
set -a; . ./.env; set +a; ./cmake-build-debug/TelegramBot
```

- Новый тестовый файл добавляй в тестовую цель в `CMakeLists.txt` — сам он не подхватится.
- В тестовые цели не добавляй `fetcher.cpp` и всё, что ходит в сеть: сеть подменяется фейковым
  `net::IFetcher`.
- Первая конфигурация качает зависимости через FetchContent (нужен интернет); из системы берутся
  libcurl и libpq (`pg_config` должен быть в PATH).
- В WSL `docker` работает, только если в Docker Desktop включена WSL integration.
- Переменные окружения: `TELEGRAM_BOT_TOKEN`, `DATABASE_URL`
  (`postgresql://user:pass@localhost:5432/db`), `KAFKA_BROKERS` (по умолчанию `localhost:9092`),
  `CHECKER_THREADS` (воркеры чекера, по умолчанию 32), `POSTGRES_USER`, `POSTGRES_PASSWORD`,
  `POSTGRES_DB` (для docker-compose). Значения — в `.env` (он в .gitignore). `.claude/settings.json` запрещает
  только инструмент Read; через shell файл не заблокирован, но правило 8 действует и там.

## Правила

1. В сеть — только через `net::IFetcher`. Юнит-тесты в сеть не ходят (пример — `MockFetcher`
   в `http/checker_test.cpp`).
2. SQL — только в `internal/db` (`PG::CheckerStore`, `PG::BotStore`); остальной код работает через
   интерфейсы из `internal/storage`. Схему меняй в `db_description.sql` идемпотентно
   (`IF NOT EXISTS`).
3. `CheckerResult` — это и сообщение в очереди чекера, и строка в `check_results`, а
   `kafka::SiteStatusEvent` — контракт между процессами. Меняешь поля — синхронно правь схему,
   SQL в `internal/db` и JSON события.
4. У каждого потока, работающего с БД, свой объект хранилища со своим `pqxx::connection`:
   соединение не потокобезопасно и между потоками не передаётся. В `site_state` пишет только поток
   записи чекера. `PG::PGStore` со статическим соединением в новом коде не используй.
5. Задача в `ThreadPool` не выпускает исключения наружу: у воркера нет `try/catch`, исключение —
   это `std::terminate` всему процессу.
6. Всё, что задача пула захватывает по ссылке, должно пережить пул. По умолчанию захватывай
   по значению (`shared_ptr` на чекер, как в `main.cpp`).
7. Сетевые запросы и запись в БД — никогда под мьютексом.
8. Секреты — только из окружения: не хардкодить, не логировать, не выводить содержимое `.env`.
9. Проект учебный: `ThreadPool`, `BoundedThreadSafeQueue`, `UniquePtr` написаны вручную намеренно.
   Не заменяй их сторонними библиотеками без спроса — чини.

### Новый тип проверки

1. `checkers/<type>/`: `config.h`, `checker.{h,cpp}` — класс `final : public IChecker` в namespace
   `<TYPE>Checker`; сеть — через `net::IFetcher` или свой интерфейс-шов, если проверка не HTTP.
2. `builder/config.h`: значение в `Builder::ConfigType` и тип в `std::variant`; ветка
   в `Builder::CreateChecker` (из-за `default` компилятор о пропуске не предупредит — вернётся
   `nullptr`).
3. `CMakeLists.txt`: `.cpp` — в цель `DevOpsBot`; `checker_test.cpp` с фейковой сетью — в тестовую
   цель.

## Стиль кода

- Форматирование — `.clang-format` (LLVM, 4 пробела, 100 колонок). Хук
  `.claude/hooks/format-changed-lines.sh` после каждой правки прогоняет `git clang-format` только
  по изменённым строкам — как VS Code с `formatOnSaveMode: modifications`. Файлы целиком
  не переформатируй: старый код не по стилю, массовый diff зашумит ревью и `git blame`.
- Имена как в коде: типы и методы — PascalCase (`Check()`, `URL()`, `Execute()`), поля — snake_case
  с `_` на конце (`url_`); контейнеры в `internal/lib` — в стиле STL (`push`, `try_pop`). Свой
  namespace на модуль: `HTTPChecker`, `Builder`, `net`, `telegram`, `PG`.
- `#pragma once`, `override` / `final`, `[[nodiscard]]` там, где результат нельзя терять.
- Комментарии и сообщения для пользователя — на русском; комментарий объясняет «почему», как
  в существующем коде.

## Процесс

- Фичи — через OpenSpec: `/opsx:propose` → `/opsx:apply` → `/opsx:archive`; артефакты на русском
  (`openspec/config.yaml`).
- После правок собери затронутые цели и прогони `ctest`. Если менялись потоки, очередь, пул или общий
  доступ к данным — отдай diff агенту `concurrency-reviewer`.
- Git: ветка на задачу → PR в `main`. Коммит и push — только по просьбе пользователя.
