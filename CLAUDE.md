# DevOpsBot — architectural contracts

This file is the source of truth for the contracts between DevOpsBot components: who owns which
data, which thread may touch what, and in which order state is persisted and delivered. Code that
changes a contract must update this file, the OpenSpec change that motivates it, and the tests that
pin the contract down.

How to build, test and run the project, coding style and process rules are in `.claude/CLAUDE.md`.
Requirements and design rationale are in `openspec/changes/site-down-notifications/` (after archiving,
in `openspec/specs/`).

## 1. System overview

Two processes that never call each other directly. They share PostgreSQL (with strict table
ownership, see §2) and talk through one Kafka topic.

```text
DevOpsBot (checker process)                        TelegramBot (bot process)
+-----------------------------------+              +------------------------------+
| main thread: Scheduler            |              | delivery thread: Notifier    |
|   | ThreadPool::Execute           |              |   event --> Subscribers()    |
|   v                               |              |         --> sendMessage      |
| ThreadPool: N workers, Check()    |              |         --> commit offset    |
|   | push                          |              |                              |
|   v                               |              | commands thread              |
| BoundedThreadSafeQueue            |              |   getUpdates --> /subscribe  |
|   | wait_and_pop                  |              |   --> ReplaceSubscriptions   |
|   v                               |              +------+---------------^-------+
| writer thread (exactly one)       |                     |               |
|   |-- TX --> PostgreSQL <---------+---------------------+               |
|   +-- status changed --> Kafka ---+--> topic site-status (key = URL) ---+
+-----------------------------------+
```

## 2. Data ownership

| Table / topic   | Written by                              | Read by                                     |
|-----------------|-----------------------------------------|---------------------------------------------|
| `sites`         | nobody at runtime (seed / manual SQL)   | checker at startup, bot when validating      |
| `check_results` | checker writer thread                   | checker main thread (failure history)        |
| `site_state`    | checker writer thread **only**          | checker writer thread (at startup)           |
| `subscriptions` | bot commands thread                     | bot delivery thread                          |
| `site-status`   | checker writer thread                   | bot delivery thread (consumer group `tgbot`) |

- The checker decides **whether** a status changed. The bot decides only **who** is told.
- The site list is fixed while processes run: the checker reads `sites` once at startup.

## 3. Checks

- `IChecker::Check()` takes nothing, returns `CheckerResult {url, http_code, is_up}` and knows
  nothing about the queue, the database or Kafka. It is `const` and may run on any pool thread.
- All network access for checks goes through `net::IFetcher`. Unit tests replace it with a fake;
  `HTTPChecker::Fetcher` (cpr, 10 s timeout) is linked into `DevOpsBot` only.
- No response (timeout, DNS, refused connection) is `http_code == 0`, `is_up == false`.
  `is_up` is `http_code == expected_http_code`.
- The fetcher is stateless, shared by all workers, and must outlive every checker and pool task.
- `Builder::ConfigFromSite` picks the check type from the URL scheme (`http://`, `https://`) and
  returns `nullopt` otherwise. `Builder::CreateChecker` never returns `nullptr` for a config produced
  by `ConfigFromSite`; unsupported sites are skipped at startup with a log line.

## 4. Status and priority semantics

Implemented as pure functions in `internal/status_checker/monitor/` and covered by unit tests.

- A site goes **down** after `FAILURES_TO_DOWN = 2` consecutive failed checks; a successful check
  resets the counter. A down site is **up** again after the first successful check.
- A site without stored state starts as up and already notified, so the first successful check is
  not a change and the first confirmed failure is.
- Only changes of `is_up` produce events; a site that stays down produces nothing.
- Polling interval: a site with failed checks within the last `FAILURE_WINDOW = 15 min` is polled
  every `max(MIN_INTERVAL = 10 s, base / 4)`, but never less often than its base interval. Intervals
  are recalculated every `RECALC_PERIOD = 30 s` from `check_results`.

## 5. Threading contracts of the checker process

- **Scheduler** runs on the main thread and owns the schedule exclusively (no mutex). The only state
  shared with workers is a per-site `std::atomic<bool> in_flight`: set before submitting a check,
  cleared by the task (release) after the result is pushed. While it is set, the site is not
  submitted again, so the pool never holds more tasks than there are sites.
- **Backpressure** therefore reaches the scheduler: slow writer → full queue → `push` blocks →
  `in_flight` stays set → the site skips its slots.
- **Pool tasks** capture everything by value (`shared_ptr` to the checker and the flag), catch every
  exception (an escaping exception is `std::terminate` for the whole process), and skip the network
  call once the stop flag is set.
- **Queue**: `BoundedThreadSafeQueue<CheckerResult>` with capacity = number of sites, many producers
  (pool workers), exactly one consumer (writer thread). After `close()`, `push` returns `false` and
  `wait_and_pop` drains the remaining items before returning `false`.
- **Database connections**: every thread that touches PostgreSQL owns its own store object with its
  own `pqxx::connection` (`PG::CheckerStore`, `PG::BotStore`). A connection is never shared between
  threads; handing one over before the using thread starts (e.g. `ResultWriter::Recover()` on the
  main thread) is allowed. No database or network call is ever made while holding a mutex.
- **Lifetimes**: the queue is declared before the pool and the writer thread so it is destroyed after
  every thread that touches it has been joined.
- **Shutdown** (SIGINT/SIGTERM set a lock-free atomic flag):
  `scheduler loop exits → ~ThreadPool (queued tasks skip work, running checks finish) →
  queue.close() → writer drains the queue and exits → join → exit 0`.
- **Writer failure** (e.g. lost database connection): the writer thread closes the queue and sets the
  stop flag; the process exits with code 1. There is no in-process reconnect — a restart recovers
  everything through §6.

## 6. Persistence and delivery order (at-least-once)

For every result the writer thread does, strictly in this order:

1. one transaction: `INSERT check_results` + `UPSERT site_state (is_up, consecutive_failures,
   changed_at)`; `notified_is_up` is not touched;
2. if `is_up != notified_is_up`: publish the event to Kafka and wait for the broker acknowledgement
   (`acks=all`, idempotent producer, 5 s timeout);
3. after the acknowledgement: `UPDATE site_state SET notified_is_up = is_up`.

Invariants:

- Never publish before the transaction is committed. (Publishing first loses the recovery message:
  after a crash the database would not know "down" was sent.)
- On startup, `ResultWriter::Recover()` publishes every row with `is_up != notified_is_up` **before**
  the scheduler starts, otherwise a site that recovered meanwhile would never get its "up" message.
- An unacknowledged event leaves the mismatch in place and is retried with the next result of the
  same site.
- In-memory state is updated only after a successful commit.

Guarantee: a status change is never lost; a duplicate message is possible after a crash between
the acknowledgement and step 3, or between `sendMessage` and the offset commit (§8).

## 7. Kafka event contract

- Topic `site-status`, one partition, message key = site URL (per-site order is preserved).
- Value is JSON, defined by `kafka::SiteStatusEvent` in `internal/kafka/site_status_event.h`:

  ```json
  {"url": "https://example.com", "is_up": false, "http_code": 503, "changed_at": 1731000000}
  ```

  `changed_at` is unix time in seconds; `http_code == 0` means no response.
- Changes must stay compatible in both directions: readers ignore unknown fields; a message that
  cannot be parsed is logged, committed and skipped so it can never block the consumer.
- The checker owns the producer; the bot reads with consumer group `tgbot`,
  `enable.auto.commit=false`, `auto.offset.reset=earliest`.

## 8. Bot contracts

- Two threads: the **commands** thread (`getUpdates` long polling → `/subscribe` → database → reply)
  and the **delivery** thread (`Notifier`). They share only the stop flag and the `telegram::Bot`
  transport, whose methods are `const` and touch only the token. Each thread owns its `PG::BotStore`.
- `/subscribe <url> [<url> ...]` replaces the user's whole subscription list in one transaction.
  `/subscribe` without URLs removes all subscriptions. URLs not in `sites` are rejected and listed in
  the reply; if every URL is rejected, subscriptions are left unchanged. Any other text gets help.
- Delivery handles events one at a time in topic order. The offset is committed only after every
  subscriber has been handled; stopping earlier leaves it uncommitted so the event is redelivered.
- `sendMessage` outcomes: HTTP 200 → sent; no response, 429 or 5xx → retry the same message
  (429 honours `retry_after`, otherwise exponential backoff up to 30 s); any other 4xx (blocked bot,
  unknown chat) → skip that recipient so one user cannot block delivery to the others.
- The bot token is part of every request URL: request URLs and cpr errors are never logged.

## 9. Storage contracts

- All SQL lives in `internal/db`. The rest of the code depends on the interfaces in
  `internal/storage` (`storage::ICheckerStore`, `storage::IBotStore`), which do not include libpqxx,
  so logic is unit-tested with in-memory fakes.
- The schema is `internal/db/db_description.sql` and must stay idempotent
  (`CREATE ... IF NOT EXISTS`, `ADD COLUMN IF NOT EXISTS`); it is applied manually.
- `CheckerResult`, the `check_results` row, `site_state` and `kafka::SiteStatusEvent` describe the
  same facts. Changing a field means changing the schema, the SQL in `internal/db`, the JSON mapping
  and the tests together.
- `PG::PGStore` / `PG::PGStorage` (static per-process connection) predate these contracts and must
  not be used by new code.
