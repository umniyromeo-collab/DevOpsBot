#include "thread_queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Пауза, за которую заблокированный поток успел бы проснуться, если бы его разбудили по ошибке.
// Проверки «поток всё ещё ждёт» не дают ложных падений: при правильной очереди поток ждёт
// сколько угодно долго, а пауза лишь даёт ошибке шанс проявиться.
constexpr auto kSettle = 100ms;

TEST(BoundedThreadSafeQueue, ZeroCapacityThrows) {
    EXPECT_THROW({ BoundedThreadSafeQueue<int> queue(0); }, std::invalid_argument);
}

TEST(BoundedThreadSafeQueue, KeepsFifoOrder) {
    BoundedThreadSafeQueue<int> queue(3);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    ASSERT_TRUE(queue.push(3));

    int value = 0;
    for (int expected : {1, 2, 3}) {
        ASSERT_TRUE(queue.wait_and_pop(value));
        EXPECT_EQ(value, expected);
    }
}

TEST(BoundedThreadSafeQueue, TryPushFailsWhenFull) {
    BoundedThreadSafeQueue<int> queue(1);
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_FALSE(queue.try_push(2));
    EXPECT_TRUE(queue.full());
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.capacity(), 1u);
}

TEST(BoundedThreadSafeQueue, TryPopFailsWhenEmpty) {
    BoundedThreadSafeQueue<int> queue(1);
    int value = -1;
    EXPECT_FALSE(queue.try_pop(value));
    EXPECT_EQ(value, -1);
    EXPECT_TRUE(queue.empty());
}

TEST(BoundedThreadSafeQueue, PushBlocksWhileFull) {
    BoundedThreadSafeQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    std::atomic<bool> pushed{false};
    std::thread producer([&] {
        EXPECT_TRUE(queue.push(2));
        pushed = true;
    });

    std::this_thread::sleep_for(kSettle);
    EXPECT_FALSE(pushed) << "push не должен проходить, пока очередь полная";

    int value = 0;
    const bool popped = queue.try_pop(value);
    if (!popped) {
        queue.close(); // иначе производитель так и будет ждать, и join зависнет
    }
    producer.join();
    ASSERT_TRUE(popped);
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(pushed);
    ASSERT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 2);
}

TEST(BoundedThreadSafeQueue, WaitAndPopBlocksUntilPush) {
    BoundedThreadSafeQueue<int> queue(1);
    std::atomic<bool> popped{false};
    int received = 0;

    std::thread consumer([&] {
        int value = 0;
        EXPECT_TRUE(queue.wait_and_pop(value));
        received = value;
        popped = true;
    });

    std::this_thread::sleep_for(kSettle);
    EXPECT_FALSE(popped) << "wait_and_pop не должен возвращаться из пустой очереди";

    const bool pushed = queue.push(42);
    if (!pushed) {
        queue.close(); // разбудить потребителя, чтобы join не завис
    }
    consumer.join();
    ASSERT_TRUE(pushed);
    EXPECT_EQ(received, 42);
}

TEST(BoundedThreadSafeQueue, CloseWakesWaitingConsumer) {
    BoundedThreadSafeQueue<int> queue(1);
    std::atomic<bool> result{true};

    std::thread consumer([&] {
        int value = 0;
        result = queue.wait_and_pop(value);
    });

    std::this_thread::sleep_for(kSettle);
    queue.close();
    consumer.join();
    EXPECT_FALSE(result);
}

TEST(BoundedThreadSafeQueue, CloseWakesAllBlockedProducers) {
    BoundedThreadSafeQueue<int> queue(1);
    ASSERT_TRUE(queue.push(0));

    std::atomic<int> rejected{0};
    std::vector<std::thread> producers;
    for (int i = 1; i <= 3; ++i) {
        producers.emplace_back([&, i] {
            if (!queue.push(i)) {
                ++rejected;
            }
        });
    }

    std::this_thread::sleep_for(kSettle);
    queue.close();
    for (auto &producer : producers) {
        producer.join();
    }
    EXPECT_EQ(rejected, 3);
    EXPECT_EQ(queue.size(), 1u);
}

TEST(BoundedThreadSafeQueue, ConsumerDrainsRestAfterClose) {
    BoundedThreadSafeQueue<int> queue(2);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    queue.close();

    int value = 0;
    ASSERT_TRUE(queue.wait_and_pop(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.wait_and_pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_FALSE(queue.wait_and_pop(value));
    EXPECT_FALSE(queue.try_pop(value));
}

TEST(BoundedThreadSafeQueue, PushAfterCloseFails) {
    BoundedThreadSafeQueue<int> queue(2);
    queue.close();
    queue.close(); // повторный вызов безопасен

    EXPECT_TRUE(queue.closed());
    EXPECT_FALSE(queue.push(1));
    EXPECT_FALSE(queue.try_push(2));
    EXPECT_TRUE(queue.empty());
}

// Стресс: много производителей на маленькой очереди и один потребитель, как в чекере
// (воркеры пула и поток записи). Ничего не теряется, не дублируется, а элементы одного
// производителя приходят в том порядке, в котором он их положил.
TEST(BoundedThreadSafeQueue, ManyProducersOneConsumer) {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 2000;
    BoundedThreadSafeQueue<int> queue(8);

    // Значение кодирует производителя и номер: p * kPerProducer + i.
    std::vector<int> last_seen(kProducers, -1);
    int count = 0;
    bool in_order = true;
    std::thread consumer([&] {
        int value = 0;
        while (queue.wait_and_pop(value)) {
            const int producer = value / kPerProducer;
            const int index = value % kPerProducer;
            in_order = in_order && index > last_seen[producer];
            last_seen[producer] = index;
            ++count;
        }
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                EXPECT_TRUE(queue.push(p * kPerProducer + i));
            }
        });
    }
    for (auto &producer : producers) {
        producer.join();
    }
    queue.close();
    consumer.join();

    EXPECT_EQ(count, kProducers * kPerProducer);
    EXPECT_TRUE(in_order);
    for (int p = 0; p < kProducers; ++p) {
        EXPECT_EQ(last_seen[p], kPerProducer - 1);
    }
}

// close() посреди активного обмена — основной сценарий остановки чекера. Всё, что push принял,
// потребитель дочитывает; всё, что отклонено, не появляется.
TEST(BoundedThreadSafeQueue, CloseDuringTraffic) {
    for (int round = 0; round < 50; ++round) {
        BoundedThreadSafeQueue<int> queue(1 + round % 4);
        std::atomic<long long> pushed_sum{0};
        std::atomic<int> pushed_count{0};
        long long popped_sum = 0;
        int popped_count = 0;

        std::thread consumer([&] {
            int value = 0;
            while (queue.wait_and_pop(value)) {
                popped_sum += value;
                ++popped_count;
            }
        });

        std::vector<std::thread> producers;
        for (int p = 0; p < 4; ++p) {
            producers.emplace_back([&] {
                for (int i = 1; queue.push(i); ++i) {
                    pushed_sum += i;
                    ++pushed_count;
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::microseconds(200 + 50 * round));
        queue.close();
        for (auto &producer : producers) {
            producer.join();
        }
        consumer.join();

        EXPECT_EQ(popped_count, pushed_count.load()) << "round " << round;
        EXPECT_EQ(popped_sum, pushed_sum.load()) << "round " << round;
    }
}
