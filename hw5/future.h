#pragma once

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T>
class Future;

namespace detail {

template <class T>
struct SharedState {
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    bool ready = false;
    std::exception_ptr exception;
    std::optional<T> value;

    void SetValue(T v) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready) {
                throw std::runtime_error("value already set");
            }
            value.emplace(std::move(v));
            ready = true;
        }
        cv.notify_all();
    }

    template <class... Args>
    void EmplaceValue(Args&&... args) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready) {
                throw std::runtime_error("value already set");
            }
            value.emplace(std::forward<Args>(args)...);
            ready = true;
        }
        cv.notify_all();
    }

    void SetException(std::exception_ptr ex) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready) {
                throw std::runtime_error("value already set");
            }
            exception = std::move(ex);
            ready = true;
        }
        cv.notify_all();
    }

    void Wait() const {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return ready; });
    }

    bool IsReady() const {
        std::lock_guard<std::mutex> lock(mutex);
        return ready;
    }
};

template <>
struct SharedState<void> {
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    bool ready = false;
    std::exception_ptr exception;

    void SetValue() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready) {
                throw std::runtime_error("value already set");
            }
            ready = true;
        }
        cv.notify_all();
    }

    void SetException(std::exception_ptr ex) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready) {
                throw std::runtime_error("value already set");
            }
            exception = std::move(ex);
            ready = true;
        }
        cv.notify_all();
    }

    void Wait() const {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return ready; });
    }

    bool IsReady() const {
        std::lock_guard<std::mutex> lock(mutex);
        return ready;
    }
};

}  

template <class T>
class Future {
public:
    Future() = default;

    explicit Future(std::shared_ptr<detail::SharedState<T>> state)
        : state_(std::move(state)) {}

    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    bool valid() const noexcept {
        return static_cast<bool>(state_);
    }

    void wait() const {
        CheckValid();
        state_->Wait();
    }

    bool is_ready() const {
        CheckValid();
        return state_->IsReady();
    }

    T get() {
        CheckValid();
        if (consumed_) {
            throw std::runtime_error("future already consumed");
        }
        consumed_ = true;
        state_->Wait();

        std::exception_ptr ex;
        std::optional<T> value;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            ex = state_->exception;
            value = std::move(state_->value);
        }

        state_.reset();

        if (ex) {
            std::rethrow_exception(ex);
        }

        return std::move(*value);
    }

private:
    void CheckValid() const {
        if (!state_) {
            throw std::runtime_error("invalid future");
        }
    }

    std::shared_ptr<detail::SharedState<T>> state_;
    bool consumed_ = false;
};

template <>
class Future<void> {
public:
    Future() = default;

    explicit Future(std::shared_ptr<detail::SharedState<void>> state)
        : state_(std::move(state)) {}

    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    bool valid() const noexcept {
        return static_cast<bool>(state_);
    }

    void wait() const {
        CheckValid();
        state_->Wait();
    }

    bool is_ready() const {
        CheckValid();
        return state_->IsReady();
    }

    void get() {
        CheckValid();
        if (consumed_) {
            throw std::runtime_error("future already consumed");
        }
        consumed_ = true;
        state_->Wait();

        std::exception_ptr ex;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            ex = state_->exception;
        }

        state_.reset();

        if (ex) {
            std::rethrow_exception(ex);
        }
    }

private:
    void CheckValid() const {
        if (!state_) {
            throw std::runtime_error("invalid future");
        }
    }

    std::shared_ptr<detail::SharedState<void>> state_;
    bool consumed_ = false;
};
