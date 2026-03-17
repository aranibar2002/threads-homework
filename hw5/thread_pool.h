#pragma once

#include "future.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace detail {

template <class F, class Tuple, std::size_t... I>
decltype(auto) ApplyMoveImpl(F&& f, Tuple&& t, std::index_sequence<I...>) {
    return std::invoke(
        std::forward<F>(f),
        std::move(std::get<I>(t))...
    );
}

template <class F, class Tuple>
decltype(auto) ApplyMove(F&& f, Tuple&& t) {
    using TupleType = std::remove_reference_t<Tuple>;
    constexpr std::size_t N = std::tuple_size_v<TupleType>;
    return ApplyMoveImpl(
        std::forward<F>(f),
        std::forward<Tuple>(t),
        std::make_index_sequence<N>{}
    );
}

struct ITask {
    virtual ~ITask() = default;
    virtual void Run() noexcept = 0;
};

template <class F>
class TaskModel final : public ITask {
public:
    explicit TaskModel(F&& f) : f_(std::move(f)) {}
    void Run() noexcept override { f_(); }

private:
    F f_;
};

}  

class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency()) {
        if (thread_count == 0) {
            thread_count = 1;
        }

        workers_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { WorkerLoop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() {
        Stop();
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }

        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    template <class F, class... Args>
    auto Submit(F&& f, Args&&... args)
        -> Future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
        using Result = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        auto state = std::make_shared<detail::SharedState<Result>>();

        auto fn = std::decay_t<F>(std::forward<F>(f));
        auto tup = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...);

        auto job = [state, fn = std::move(fn), tup = std::move(tup)]() mutable noexcept {
            try {
                if constexpr (std::is_void_v<Result>) {
                    detail::ApplyMove(std::move(fn), std::move(tup));
                    state->SetValue();
                } else {
                    state->SetValue(detail::ApplyMove(std::move(fn), std::move(tup)));
                }
            } catch (...) {
                try {
                    state->SetException(std::current_exception());
                } catch (...){
                }
            } 
        };

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks_.push_back(
                std::make_unique<detail::TaskModel<decltype(job)>>(std::move(job))
            );
        }

        cv_.notify_one();
        return Future<Result>(std::move(state));
    }

    std::size_t ThreadCount() const noexcept {
        return workers_.size();
    }

private:
  void WorkerLoop() noexcept {
    for (;;) {
        try {
            std::unique_ptr<detail::ITask> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });

                if (stopping_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop_front();
            }

            task->Run();
        } catch (...) {
            std::terminate();
        }
    }
}

    std::vector<std::thread> workers_;
    std::deque<std::unique_ptr<detail::ITask>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};
