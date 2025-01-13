//
// Created by qianlu on 2024/5/21.
//

#pragma once

#include <pthread.h>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/type/NetworkObserverEvent.h"

namespace logtail {
namespace ebpf {

class NetDataEvent;
class AbstractRecord;

template <typename Event, typename Res>
class WorkerFunc {
public:
    using EventQueue = moodycamel::BlockingConcurrentQueue<Event>;
    using ResultQueue = moodycamel::BlockingConcurrentQueue<Res>;

    virtual void operator()(Event& evt, ResultQueue& resQueue) = 0;
};

template <typename Event, typename Res>
class WorkerPool {
public:
    using EventQueue = moodycamel::BlockingConcurrentQueue<Event>;
    using ResultQueue = moodycamel::BlockingConcurrentQueue<Res>;
    using Func = std::function<void(Event&, ResultQueue&)>;

    WorkerPool(EventQueue& eventQueue, ResultQueue& resultQueue, Func&& func, size_t threadCount)
        : eventQueue_(eventQueue), resultQueue_(resultQueue), func_(func), threadCount_(threadCount), done_(false) {
        for (size_t i = 0; i < threadCount_; ++i) {
            workers_.emplace_back(&WorkerPool::workerThread, this);
        }
    }

    ~WorkerPool() {
        done_ = true;
        eventQueue_.enqueue(Event{}); // Push dummy events to wake up any waiting threads
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // non copyable
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

private:
    void workerThread() {
        while (!done_) {
            Event event;
            // TODO @qianlu.kk make it configurable
            if (eventQueue_.wait_dequeue_timed(event, std::chrono::milliseconds(200))) {
                if (done_)
                    break; // Early exit if done_ is true
                func_(event, resultQueue_);
            }
        }
    }

    std::vector<std::thread> workers_;
    EventQueue& eventQueue_;
    ResultQueue& resultQueue_;
    Func func_;
    size_t threadCount_;
    std::atomic<bool> done_;
};

class NetDataHandler : public WorkerFunc<std::unique_ptr<NetDataEvent>, std::shared_ptr<AbstractRecord>> {
public:
    void operator()(std::unique_ptr<NetDataEvent>& evt, ResultQueue& resQueue) override;
    ~NetDataHandler() {}
    NetDataHandler(const NetDataHandler&& other) : count_(other.count_) {}
    NetDataHandler() {}
    NetDataHandler(const NetDataHandler& other) : count_(other.count_) {}

private:
    int64_t count_ = 0;
};

} // namespace ebpf
} // namespace logtail
