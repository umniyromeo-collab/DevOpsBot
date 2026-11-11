#include "result_writer.h"

#include "status_tracker.h"

#include <chrono>
#include <iostream>
#include <utility>

namespace monitor {

    ResultWriter::ResultWriter(storage::ICheckerStore &store, kafka::IEventPublisher &publisher,
                               UnixClock now)
        : store_(store), publisher_(publisher), now_(std::move(now)) {}

    std::int64_t ResultWriter::DefaultClock() {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    void ResultWriter::Recover() {
        statuses_ = store_.LoadStatuses();
        for (auto &[url, status] : statuses_) {
            PublishIfPending(url, status);
        }
    }

    void ResultWriter::Process(const CheckerResult &result) {
        const auto it = statuses_.find(result.url);
        const SiteStatus current = it != statuses_.end() ? it->second : SiteStatus{};

        StatusUpdate update = ApplyCheck(current, result.is_up, result.http_code);
        if (update.changed) {
            update.status.changed_at = now_();
            std::cout << result.url << (update.status.is_up ? ": восстановился" : ": упал")
                      << " (код " << result.http_code << ")" << std::endl;
        }

        // Сначала коммит, потом память: упадёт транзакция — копия в памяти останется верной.
        store_.SaveResult(result, update.status);
        SiteStatus &status = statuses_[result.url] = update.status;

        PublishIfPending(result.url, status);
    }

    void ResultWriter::Run(BoundedThreadSafeQueue<CheckerResult> &queue) {
        CheckerResult result;
        while (queue.wait_and_pop(result)) {
            Process(result);
        }
    }

    void ResultWriter::PublishIfPending(const std::string &url, SiteStatus &status) {
        if (status.is_up == status.notified_is_up) {
            return;
        }
        const kafka::SiteStatusEvent event{url, status.is_up, status.last_http_code,
                                           status.changed_at};
        if (!publisher_.Publish(event)) {
            // Расхождение остаётся в БД, событие уйдёт со следующим результатом этого сайта.
            std::cerr << url << ": Kafka не подтвердила событие, повторим со следующей проверкой"
                      << std::endl;
            return;
        }
        store_.MarkNotified(url, status.is_up);
        status.notified_is_up = status.is_up;
    }

} // namespace monitor
