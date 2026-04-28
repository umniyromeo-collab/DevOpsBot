# Многопоточность в C++

## Содержание

1. [Введение](#1-введение)
2. [std::thread — создание потоков](#2-stdthread--создание-потоков)
3. [Data Race — гонка данных](#3-data-race--гонка-данных)
4. [std::mutex и RAII-обёртки](#4-stdmutex-и-raii-обёртки)
5. [Deadlock](#5-deadlock)
6. [std::atomic](#6-stdatomic)
7. [std::condition_variable](#7-stdcondition_variable)
8. [std::async и std::future](#8-stdasync-и-stdfuture)
9. [C++20/23: jthread, семафоры, latch, barrier](#9-c2023-jthread-семафоры-latch-barrier)
10. [Частые ошибки и best practices](#10-частые-ошибки-и-best-practices)
11. [Практическое задание: ThreadSafeQueue](#11-практическое-задание-threadsafequeue)

---

## 1. Введение

### Зачем нужна многопоточность?

Современные процессоры имеют несколько ядер. Однопоточная программа использует только
одно ядро, остальные простаивают. Многопоточность позволяет:

- **Ускорить вычисления** — разделить работу между ядрами (parallelism)
- **Повысить отзывчивость** — UI-поток не блокируется, пока фоновый поток загружает данные
- **Обрабатывать несколько клиентов** — веб-сервер обслуживает запросы параллельно

### Параллелизм vs конкурентность

| | Параллелизм (Parallelism) | Конкурентность (Concurrency) |
|---|---|---|
| **Суть** | Одновременное выполнение задач на разных ядрах | Управление несколькими задачами, которые могут чередоваться |
| **Пример** | 4 потока суммируют 4 части массива одновременно | Веб-сервер переключается между обработкой запросов |
| **Требования** | Несколько ядер CPU | Достаточно одного ядра |

### Закон Амдала

Максимальное ускорение программы ограничено её последовательной частью:

```
Ускорение = 1 / (S + (1 - S) / N)
```

Где `S` — доля последовательного кода, `N` — количество потоков.

Если 10% кода нельзя распараллелить, максимальное ускорение — **10x**, даже с бесконечным
числом ядер.

### Модель памяти C++

Начиная с C++11, язык имеет формальную модель памяти:

- Каждый поток имеет **свой стек** (локальные переменные)
- **Куча (heap) общая** — все потоки видят одни и те же объекты в динамической памяти
- Одновременный доступ к данным без синхронизации — **undefined behavior**

---

## 2. std::thread — создание потоков

### Заголовок

```cpp
#include <thread>
```

### Создание потока

Поток создаётся из любого вызываемого объекта: функции, лямбды, функтора.

```cpp
#include <thread>
#include <iostream>

void worker(int id) {
    std::cout << "Thread " << id << " is working\n";
}

int main() {
    std::thread t1(worker, 1);  // запускает worker(1) в новом потоке
    std::thread t2(worker, 2);  // запускает worker(2) в новом потоке

    t1.join();  // главный поток ждёт завершения t1
    t2.join();  // главный поток ждёт завершения t2
}
```

### Создание через лямбду

```cpp
int main() {
    int x = 42;

    std::thread t([x]() {
        std::cout << "Lambda thread, x = " << x << "\n";
    });

    t.join();
}
```

### join() vs detach()

| Метод | Что делает | Когда использовать |
|---|---|---|
| `join()` | Блокирует вызывающий поток до завершения целевого | Когда нужен результат или гарантия завершения |
| `detach()` | Отделяет поток, он работает самостоятельно | Фоновые задачи (логирование, мониторинг). Используется редко |

**Критически важно:** если объект `std::thread` уничтожается без вызова `join()` или
`detach()`, программа аварийно завершается через `std::terminate()`.

```cpp
void dangerous() {
    std::thread t([] { /* ... */ });
    // t уничтожается без join/detach → std::terminate()!
}
```

### Передача аргументов

```cpp
void by_value(int x);           // копируется
void by_ref(int& x);            // ОШИБКА без std::ref!
void by_move(std::string&& s);  // нужен std::move

int n = 10;
std::string str = "hello";

std::thread t1(by_value, n);              // OK — копия
std::thread t2(by_ref, std::ref(n));      // OK — ссылка через std::ref
std::thread t3(by_move, std::move(str));  // OK — перемещение
```

**Важно:** `std::thread` по умолчанию **копирует** все аргументы. Для передачи по ссылке
нужен `std::ref()`. Без него код скомпилируется, но функция получит копию, а не ссылку —
источник тонких багов.

### hardware_concurrency()

```cpp
unsigned int n = std::thread::hardware_concurrency();
// Возвращает количество аппаратных потоков (обычно = числу ядер * гипертрединг)
// Может вернуть 0, если информация недоступна
```

---

## 3. Data Race — гонка данных

### Что это?

Data race возникает, когда:
1. Два или более потоков обращаются к одной переменной
2. Хотя бы один из них пишет
3. Нет синхронизации между ними

Это **undefined behavior** — программа может дать неверный результат, упасть или казаться
рабочей (до продакшена).

### Демонстрация

```cpp
#include <thread>
#include <iostream>

int counter = 0;

void increment(int n) {
    for (int i = 0; i < n; ++i) {
        ++counter;  // DATA RACE!
    }
}

int main() {
    std::thread t1(increment, 1'000'000);
    std::thread t2(increment, 1'000'000);

    t1.join();
    t2.join();

    std::cout << "Counter = " << counter << "\n";
    // Ожидаем 2'000'000, но получим меньше — например, 1'547'832
}
```

### Почему так происходит?

Операция `++counter` на уровне процессора — это три шага:

```
1. READ:  загрузить counter из памяти в регистр
2. MODIFY: увеличить значение в регистре на 1
3. WRITE: записать результат обратно в память
```

Если два потока выполнят READ одновременно, оба прочитают одно и то же значение,
оба увеличат его на 1, и оба запишут одинаковый результат. Одно инкрементирование потеряно.

```
Поток 1:  READ(0)  →  MODIFY(1)  →  WRITE(1)
Поток 2:       READ(0)  →  MODIFY(1)  →  WRITE(1)
                                                ↑
                                    counter = 1, а не 2!
```

### Обнаружение

Компилятор может помочь:

```bash
# Компиляция с Thread Sanitizer (Clang / GCC)
g++ -fsanitize=thread -g -o prog prog.cpp -lpthread
```

ThreadSanitizer (TSan) обнаруживает data race в рантайме и выдаёт подробный отчёт.

---

## 4. std::mutex и RAII-обёртки

### std::mutex

Мьютекс (mutual exclusion) — примитив синхронизации, гарантирующий, что только один поток
одновременно выполняет защищённый участок кода (критическую секцию).

```cpp
#include <mutex>

std::mutex mtx;
int counter = 0;

void safe_increment(int n) {
    for (int i = 0; i < n; ++i) {
        mtx.lock();
        ++counter;
        mtx.unlock();
    }
}
```

**Проблема:** если между `lock()` и `unlock()` вылетит исключение, мьютекс останется
заблокированным навсегда. Решение — RAII.

### RAII-обёртки

#### std::lock_guard (C++11)

Простейшая обёртка. Захватывает мьютекс в конструкторе, отпускает в деструкторе.

```cpp
void safe_increment(int n) {
    for (int i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lock(mtx);  // захват
        ++counter;
    }  // автоматический unlock при выходе из scope
}
```

С C++17 можно не указывать шаблонный параметр (CTAD):

```cpp
std::lock_guard lock(mtx);
```

#### std::unique_lock (C++11)

Более гибкая обёртка — можно отложить захват, отпустить вручную, передать владение.

```cpp
std::unique_lock<std::mutex> lock(mtx);             // сразу захватить
std::unique_lock<std::mutex> lock(mtx, std::defer_lock);  // создать без захвата
lock.lock();    // захватить позже
lock.unlock();  // отпустить вручную (не дожидаясь деструктора)
```

Необходим для работы с `std::condition_variable`.

#### std::scoped_lock (C++17)

Захватывает **несколько мьютексов** одновременно, без риска deadlock:

```cpp
std::mutex mtx1, mtx2;

void transfer() {
    std::scoped_lock lock(mtx1, mtx2);  // оба захвачены атомарно
    // ... безопасная работа ...
}
```

### Сводная таблица

| Обёртка | Стандарт | Несколько мьютексов | Отложенный lock | Ручной unlock |
|---|---|---|---|---|
| `lock_guard` | C++11 | Нет | Нет | Нет |
| `unique_lock` | C++11 | Нет | Да | Да |
| `scoped_lock` | C++17 | Да | Нет | Нет |

**Правило:** используй `lock_guard` по умолчанию, `scoped_lock` для нескольких мьютексов,
`unique_lock` для condition_variable и нестандартных сценариев.

---

## 5. Deadlock

### Что это?

Deadlock — ситуация, когда два или более потоков ждут друг друга и ни один не может
продолжить работу.

### Классический пример

```cpp
std::mutex mtx_a, mtx_b;

void thread1() {
    std::lock_guard lock_a(mtx_a);  // захватил A
    // ... некоторая работа ...
    std::lock_guard lock_b(mtx_b);  // ждёт B → DEADLOCK
}

void thread2() {
    std::lock_guard lock_b(mtx_b);  // захватил B
    // ... некоторая работа ...
    std::lock_guard lock_a(mtx_a);  // ждёт A → DEADLOCK
}
```

Поток 1 держит A и ждёт B. Поток 2 держит B и ждёт A. Оба заблокированы навечно.

### Как избежать

1. **Фиксированный порядок захвата** — всегда захватывать мьютексы в одном порядке (A, затем B)
2. **std::scoped_lock** — захватывает несколько мьютексов атомарно, внутри использует
   алгоритм предотвращения deadlock
3. **std::lock()** + `std::adopt_lock` — для C++11/14:

```cpp
void safe() {
    std::lock(mtx_a, mtx_b);  // захватывает оба без deadlock
    std::lock_guard lock_a(mtx_a, std::adopt_lock);
    std::lock_guard lock_b(mtx_b, std::adopt_lock);
    // ...
}
```

4. **Минимизировать критические секции** — чем меньше кода под мьютексом, тем меньше
   вероятность вложенных захватов

---

## 6. std::atomic

### Заголовок

```cpp
#include <atomic>
```

### Зачем?

Атомарные операции выполняются как единое целое — другие потоки не могут увидеть
промежуточное состояние. Не нужен мьютекс для простых операций.

### Пример

```cpp
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> counter{0};

void increment(int n) {
    for (int i = 0; i < n; ++i) {
        counter.fetch_add(1);  // атомарный инкремент
        // или просто: ++counter;
    }
}

int main() {
    std::thread t1(increment, 1'000'000);
    std::thread t2(increment, 1'000'000);
    t1.join();
    t2.join();

    std::cout << "Counter = " << counter << "\n";
    // Всегда 2'000'000 — гарантированно
}
```

### Основные операции

```cpp
std::atomic<int> x{0};

x.store(42);           // записать
int val = x.load();    // прочитать
int old = x.exchange(10);  // записать и вернуть старое значение

// Compare-and-swap (CAS) — основа lock-free алгоритмов
int expected = 42;
bool ok = x.compare_exchange_strong(expected, 100);
// Если x == 42, записывает 100 и возвращает true
// Иначе записывает текущее значение x в expected и возвращает false

x.fetch_add(1);  // атомарный x += 1
x.fetch_sub(1);  // атомарный x -= 1
```

### Memory ordering (кратко)

По умолчанию все атомарные операции используют `std::memory_order_seq_cst` (sequentially
consistent) — самый строгий порядок, самый безопасный, но самый медленный.

| Порядок | Гарантии | Когда |
|---|---|---|
| `seq_cst` | Все потоки видят одинаковый порядок операций | По умолчанию, когда не уверены |
| `acquire` / `release` | Пара: release публикует данные, acquire их видит | Producer-consumer |
| `relaxed` | Только атомарность, без гарантий порядка | Счётчики, статистика |

**Совет:** начинайте с `seq_cst` (по умолчанию). Оптимизируйте порядок только если
профайлер показал bottleneck.

### atomic vs mutex — когда что?

| Критерий | `std::atomic` | `std::mutex` |
|---|---|---|
| Тип данных | Примитивы (int, bool, ptr) | Любые структуры |
| Операция | Одна атомарная операция | Несколько связанных операций |
| Скорость | Быстрее (без context switch) | Медленнее (системный вызов) |
| Пример | Счётчик, флаг | Обновление структуры с несколькими полями |

### std::atomic<bool> как флаг остановки

```cpp
std::atomic<bool> stop_flag{false};

void worker() {
    while (!stop_flag.load()) {
        // ... работаем ...
    }
}

int main() {
    std::thread t(worker);

    // ... через какое-то время ...
    stop_flag.store(true);  // сигнал остановки
    t.join();
}
```

---

## 7. std::condition_variable

### Заголовок

```cpp
#include <condition_variable>
```

### Зачем?

Condition variable позволяет потоку **заснуть** и проснуться, когда другой поток
**уведомит** о наступлении условия. Без этого пришлось бы крутиться в цикле (busy wait),
впустую расходуя CPU.

### Паттерн Producer-Consumer

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <iostream>

std::mutex mtx;
std::condition_variable cv;
std::queue<int> tasks;
bool finished = false;

void producer() {
    for (int i = 0; i < 10; ++i) {
        {
            std::lock_guard lock(mtx);
            tasks.push(i);
            std::cout << "Produced: " << i << "\n";
        }
        cv.notify_one();  // будим один ожидающий поток
    }

    {
        std::lock_guard lock(mtx);
        finished = true;
    }
    cv.notify_all();  // будим всех — работа закончена
}

void consumer(int id) {
    while (true) {
        std::unique_lock lock(mtx);

        // wait() атомарно: отпускает мьютекс и засыпает
        // При пробуждении захватывает мьютекс обратно
        cv.wait(lock, [] {
            return !tasks.empty() || finished;
        });

        if (tasks.empty() && finished) {
            break;  // больше задач не будет
        }

        int task = tasks.front();
        tasks.pop();
        lock.unlock();  // отпускаем мьютекс до обработки

        std::cout << "Consumer " << id << " processed: " << task << "\n";
    }
}

int main() {
    std::thread prod(producer);
    std::thread cons1(consumer, 1);
    std::thread cons2(consumer, 2);

    prod.join();
    cons1.join();
    cons2.join();
}
```

### Почему предикат в wait() обязателен?

```cpp
// ПЛОХО — может пропустить уведомление или проснуться ложно
cv.wait(lock);

// ХОРОШО — проверяет условие при каждом пробуждении
cv.wait(lock, [] { return !tasks.empty() || finished; });
```

Два проблемных сценария без предиката:
1. **Spurious wakeup** — ОС может разбудить поток без notify (особенность реализации)
2. **Lost wakeup** — notify_one() вызван до wait() — уведомление потеряно

Предикат защищает от обоих: поток проверяет условие и, если оно ложно, засыпает снова.

### notify_one() vs notify_all()

| Метод | Действие | Когда использовать |
|---|---|---|
| `notify_one()` | Будит один ожидающий поток | Одна задача — один обработчик |
| `notify_all()` | Будит все ожидающие потоки | Изменилось общее состояние (например, `finished = true`) |

---

## 8. std::async и std::future

### Заголовок

```cpp
#include <future>
```

### Зачем?

`std::async` — высокоуровневый способ запустить задачу асинхронно и получить результат.
Не нужно вручную создавать потоки и синхронизировать доступ к результату.

### Пример

```cpp
#include <future>
#include <iostream>

int heavy_computation(int x) {
    // ... долгие вычисления ...
    return x * x;
}

int main() {
    // Запускаем задачу асинхронно
    std::future<int> result = std::async(std::launch::async, heavy_computation, 42);

    // ... пока задача работает, делаем другую работу ...
    std::cout << "Doing other work...\n";

    // Получаем результат (блокируемся, если задача ещё не завершена)
    int value = result.get();
    std::cout << "Result = " << value << "\n";
}
```

### Политики запуска

```cpp
// Гарантированно в отдельном потоке
auto f1 = std::async(std::launch::async, func);

// Отложенное выполнение — func() вызовется при .get()
auto f2 = std::async(std::launch::deferred, func);

// На усмотрение реализации (по умолчанию)
auto f3 = std::async(func);
```

**Совет:** всегда явно указывайте `std::launch::async`, если хотите параллельное выполнение.
Без этого стандарт разрешает отложенное выполнение.

### std::promise + std::future

Для ручного управления, когда результат устанавливается не через return:

```cpp
#include <future>
#include <thread>

void worker(std::promise<int> prom) {
    // ... делаем работу ...
    prom.set_value(42);  // передаём результат
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();

    std::thread t(worker, std::move(prom));

    int result = fut.get();  // получаем результат
    t.join();
}
```

### Исключения через future

Если задача бросает исключение, оно сохраняется в future и перебрасывается при вызове `.get()`:

```cpp
auto fut = std::async(std::launch::async, [] {
    throw std::runtime_error("oops");
    return 42;
});

try {
    int val = fut.get();  // перебрасывает исключение
} catch (const std::exception& e) {
    std::cerr << "Caught: " << e.what() << "\n";
}
```

### Параллельные вычисления с async

```cpp
#include <future>
#include <vector>
#include <numeric>

long long parallel_sum(const std::vector<int>& data) {
    size_t mid = data.size() / 2;

    auto left = std::async(std::launch::async, [&]() {
        return std::accumulate(data.begin(), data.begin() + mid, 0LL);
    });

    auto right = std::async(std::launch::async, [&]() {
        return std::accumulate(data.begin() + mid, data.end(), 0LL);
    });

    return left.get() + right.get();
}
```

---

## 9. C++20/23: jthread, семафоры, latch, barrier

### std::jthread (C++20)

Улучшенная версия `std::thread`:
- Автоматический `join()` в деструкторе — нет риска `std::terminate()`
- Встроенная поддержка **кооперативной отмены** через `stop_token`

```cpp
#include <thread>
#include <iostream>

void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        std::cout << "Working...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "Stopped gracefully\n";
}

int main() {
    std::jthread t(worker);  // stop_token передаётся автоматически

    std::this_thread::sleep_for(std::chrono::seconds(2));

    t.request_stop();  // просим поток остановиться
    // деструктор jthread вызовет join() автоматически
}
```

### std::counting_semaphore / std::binary_semaphore (C++20)

Семафор — счётчик, ограничивающий количество потоков, которые одновременно проходят
через критическую секцию.

```cpp
#include <semaphore>
#include <thread>
#include <iostream>
#include <vector>

// Разрешаем максимум 3 потока одновременно
std::counting_semaphore<3> sem(3);

void limited_access(int id) {
    sem.acquire();  // уменьшает счётчик (блокируется, если 0)
    std::cout << "Thread " << id << " in critical section\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    sem.release();  // увеличивает счётчик
}

int main() {
    std::vector<std::jthread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(limited_access, i);
    }
}
```

`std::binary_semaphore` — это `std::counting_semaphore<1>`.

### std::latch (C++20)

Одноразовый барьер: потоки уменьшают счётчик, один или несколько потоков ждут, пока он
не дойдёт до нуля.

```cpp
#include <latch>
#include <thread>
#include <iostream>
#include <vector>

int main() {
    constexpr int num_workers = 5;
    std::latch work_done(num_workers);

    auto worker = [&](int id) {
        std::cout << "Worker " << id << " done\n";
        work_done.count_down();  // уменьшает счётчик на 1
    };

    std::vector<std::jthread> threads;
    for (int i = 0; i < num_workers; ++i) {
        threads.emplace_back(worker, i);
    }

    work_done.wait();  // ждём, пока все воркеры закончат
    std::cout << "All workers finished!\n";
}
```

### std::barrier (C++20)

Многоразовый барьер: потоки собираются в точке синхронизации, затем все продолжают
одновременно. Можно использовать повторно (в отличие от latch).

```cpp
#include <barrier>
#include <thread>
#include <iostream>
#include <vector>

int main() {
    constexpr int num_threads = 4;

    // Функция вызывается когда все потоки достигли барьера
    auto on_completion = []() noexcept {
        std::cout << "--- All threads synced ---\n";
    };

    std::barrier sync_point(num_threads, on_completion);

    auto worker = [&](int id) {
        for (int phase = 0; phase < 3; ++phase) {
            std::cout << "Thread " << id << " phase " << phase << "\n";
            sync_point.arrive_and_wait();  // ждём остальных
        }
    };

    std::vector<std::jthread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
}
```

---

## 10. Частые ошибки и best practices

### Ошибки

#### 1. Забыли join()/detach()

```cpp
void bad() {
    std::thread t([] { /* ... */ });
    // t уничтожается → std::terminate()
}
```

**Решение:** используйте `std::jthread` (C++20) или всегда вызывайте `join()`/`detach()`.

#### 2. Dangling reference — захват локальной переменной по ссылке

```cpp
void bad() {
    int local = 42;
    std::thread t([&local] {
        // local может быть уже уничтожена,
        // если bad() завершится раньше потока
        std::cout << local;
    });
    t.detach();  // поток продолжает работать после выхода из bad()
}
```

**Решение:** передавайте данные по значению или гарантируйте время жизни.

#### 3. Мьютекс в цикле — слишком крупная блокировка

```cpp
// ПЛОХО — мьютекс захвачен на весь цикл, параллелизма нет
std::lock_guard lock(mtx);
for (int i = 0; i < 1'000'000; ++i) {
    ++counter;
}

// ЛУЧШЕ — используйте atomic или захватывайте мьютекс порциями
```

#### 4. Двойной lock — рекурсивный захват обычного мьютекса

```cpp
std::mutex mtx;

void a() {
    std::lock_guard lock(mtx);
    b();  // DEADLOCK — b() тоже захватывает mtx
}

void b() {
    std::lock_guard lock(mtx);  // тот же мьютекс → зависание
}
```

**Решение:** `std::recursive_mutex` (крайняя мера) или рефакторинг кода.

### Best practices

1. **Минимизируй shared state** — чем меньше данных разделяют потоки, тем проще
2. **Предпочитай message passing** — передавай данные через очереди, а не через общие переменные
3. **Критические секции — минимальные** — под мьютексом только то, что необходимо
4. **Не смешивай мьютексы и атомики для одних данных** — выбери одно
5. **Используй RAII** — никогда не вызывай `lock()`/`unlock()` напрямую
6. **Тестируй с ThreadSanitizer** — data race невоспроизводимы вручную

---

## 11. Практическое задание: ThreadSafeQueue

Реализовать потокобезопасную очередь как шаблонный класс.

### Интерфейс

```cpp
template<typename T>
class ThreadSafeQueue {
public:
    // Добавить элемент в очередь (потокобезопасно)
    void push(T value);

    // Попытаться извлечь элемент. Возвращает false, если очередь пуста
    bool try_pop(T& value);

    // Извлечь элемент с ожиданием. Блокируется, пока очередь пуста
    void wait_and_pop(T& value);

    // Проверить, пуста ли очередь
    bool empty() const;

    // Текущий размер очереди
    size_t size() const;
};
```

### Требования

1. Все методы должны быть потокобезопасными
2. `wait_and_pop` должен использовать `std::condition_variable` (не busy wait)
3. `push` должен уведомлять ожидающих в `wait_and_pop`

### Тесты (Google Test)

```cpp
// 1. Базовая функциональность — один поток
TEST(ThreadSafeQueue, SingleThreadPushPop) {
    ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    int val;
    ASSERT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);
    ASSERT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 2);
    EXPECT_EQ(q.size(), 1);
}

// 2. try_pop на пустой очереди
TEST(ThreadSafeQueue, TryPopEmpty) {
    ThreadSafeQueue<int> q;
    int val;
    EXPECT_FALSE(q.try_pop(val));
}

// 3. Многопоточность — ни один элемент не теряется и не дублируется
TEST(ThreadSafeQueue, MultiProducerMultiConsumer) {
    ThreadSafeQueue<int> q;
    constexpr int num_producers = 4;
    constexpr int num_consumers = 4;
    constexpr int items_per_producer = 10000;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::atomic<int> total_consumed{0};

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&q, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                q.push(i * items_per_producer + j);
            }
        });
    }

    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&q, &total_consumed]() {
            int val;
            while (total_consumed.load() < num_producers * items_per_producer) {
                if (q.try_pop(val)) {
                    total_consumed.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(total_consumed.load(), num_producers * items_per_producer);
    EXPECT_TRUE(q.empty());
}

// 4. wait_and_pop просыпается при push
TEST(ThreadSafeQueue, WaitAndPopWakesUp) {
    ThreadSafeQueue<int> q;

    std::thread consumer([&q]() {
        int val;
        q.wait_and_pop(val);  // заблокируется
        EXPECT_EQ(val, 42);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    q.push(42);  // разбудит consumer

    consumer.join();
}
```

### Подсказки к реализации

- Используй `std::queue<T>` как внутреннее хранилище
- `mutable std::mutex` для защиты данных (mutable — чтобы const-методы могли захватывать)
- `std::condition_variable` для `wait_and_pop`
- `push` вызывает `cv.notify_one()` после добавления элемента
