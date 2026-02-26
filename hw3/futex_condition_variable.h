#pragma once


#include <atomic>          
#include <cerrno>          
#include <climits>         
#include <cstdint>          
#include <mutex>            
#include <system_error>     
#include <chrono>           
#include <linux/futex.h>    
#include <sys/syscall.h>    
#include <unistd.h>         


class FutexConditionVariable {
public:
    FutexConditionVariable() = default;
    FutexConditionVariable(const FutexConditionVariable&) = delete;
    FutexConditionVariable& operator=(const FutexConditionVariable&) = delete;

    // notify_one: будим один ожидающий поток
 
    void notify_one() noexcept {
     
        seq.fetch_add(1, std::memory_order_release);

        // Будим ровно 1 поток, который спит на futex
        futex_wake(&seq, 1);
    }

    // notify_all: будим всех ожидающих потоков
   
    void notify_all() noexcept {
        seq.fetch_add(1, std::memory_order_release);
        futex_wake(&seq, INT_MAX); 
    }

   
    // wait без предиката 
    template <class Mutex>
    void wait(std::unique_lock<Mutex>& lock) {
        
        const uint32_t observed = seq.load(std::memory_order_acquire);
        lock.unlock();
      
        futex_wait(&seq, observed);

        lock.lock();
    }

    // wait с предикатом: правильный способ использования condvar.

    template <class Mutex, class Predicate>
    void wait(std::unique_lock<Mutex>& lock, Predicate pred) {
       
        while (!pred()) {
            wait(lock);
        }
    }

    // wait_for / wait_until 
   
    template <class Mutex, class Rep, class Period>
    bool wait_for(std::unique_lock<Mutex>& lock,
                  const std::chrono::duration<Rep, Period>& dur) {
        return wait_until(lock, std::chrono::steady_clock::now() + dur);
    }

    template <class Mutex, class Clock, class Duration>
    bool wait_until(std::unique_lock<Mutex>& lock,
                    const std::chrono::time_point<Clock, Duration>& deadline) {
    
        const uint32_t observed = seq.load(std::memory_order_acquire);


        for (;;) {
            const auto now = Clock::now();
            if (now >= deadline) {
                return false; 
            }

            const auto remaining = deadline - now;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count();

            timespec ts;
            ts.tv_sec = static_cast<time_t>(ns / 1000000000LL);
            ts.tv_nsec = static_cast<long>(ns % 1000000000LL);

            lock.unlock();
            const int rc = futex_wait_timed(&seq, observed, &ts);
            lock.lock();

            if (rc == 0) {
                return true; 
            }
            if (rc == -1 && errno == ETIMEDOUT) {
                return false; 
            }
            if (rc == -1 && errno == EAGAIN) {
             
                return true;
            }

        }
    }

private:
    
    std::atomic<uint32_t> seq{0};

    // Обёртки над futex syscall

    static int futex_wait(std::atomic<uint32_t>* addr, uint32_t expected) noexcept {
        
        auto* uaddr = reinterpret_cast<uint32_t*>(addr);

        return static_cast<int>(syscall(SYS_futex, uaddr, FUTEX_WAIT, expected, nullptr, nullptr, 0));
    }

    // futex_wait с таймаутом (relative timespec)
    static int futex_wait_timed(std::atomic<uint32_t>* addr, uint32_t expected, const timespec* ts) noexcept {
        auto* uaddr = reinterpret_cast<uint32_t*>(addr);
        return static_cast<int>(syscall(SYS_futex, uaddr, FUTEX_WAIT, expected, ts, nullptr, 0));
    }

    static int futex_wake(std::atomic<uint32_t>* addr, int count) noexcept {
        auto* uaddr = reinterpret_cast<uint32_t*>(addr);
        return static_cast<int>(syscall(SYS_futex, uaddr, FUTEX_WAKE, count, nullptr, nullptr, 0));
    }
};
