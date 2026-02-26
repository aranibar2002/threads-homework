#include "futex_condition_variable.h"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>


static void SleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int main() {
   
    // Тест 1: notify_one будит по одному
  
    {
        std::cout << "[Test1] notify_one wakes one waiter...\n";

        FutexConditionVariable cv;       
        std::mutex m;                    
        std::atomic<int> woken{0};         
        bool ready = false;              

        const int N = 8;                  
        std::vector<std::thread> th;
        th.reserve(N);

        for (int i = 0; i < N; ++i) {
            th.emplace_back([&] {
                std::unique_lock<std::mutex> lock(m);  
                cv.wait(lock, [&] { return ready; });  
                
                woken.fetch_add(1, std::memory_order_relaxed);
            });
        }

        SleepMs(50); 

        for (int k = 0; k < N; ++k) {
            {
                std::lock_guard<std::mutex> lock(m);
                ready = true; 
            }
            cv.notify_one(); 

            while (woken.load(std::memory_order_relaxed) < k + 1) {
                std::this_thread::yield();
            }

            
            {
                std::lock_guard<std::mutex> lock(m);
                ready = false;
            }
        }

        for (auto& t : th) t.join();
        std::cout << "  woken=" << woken.load() << " (expected " << N << ")\n";
    }

    // Тест 2: notify_all будит всех

    {
        std::cout << "[Test2] notify_all wakes all waiters...\n";

        FutexConditionVariable cv;
        std::mutex m;
        bool ready = false;
        std::atomic<int> woken{0};

        const int N = 12;
        std::vector<std::thread> th;
        th.reserve(N);

        for (int i = 0; i < N; ++i) {
            th.emplace_back([&] {
                std::unique_lock<std::mutex> lock(m);
                cv.wait(lock, [&] { return ready; });
                woken.fetch_add(1, std::memory_order_relaxed);
            });
        }

        SleepMs(50);

        {
            std::lock_guard<std::mutex> lock(m);
            ready = true; 
        }
        cv.notify_all(); 

        for (auto& t : th) t.join();
        std::cout << "  woken=" << woken.load() << " (expected " << N << ")\n";
    }

    
    // Тест 3: Stress (много циклов ожидания/пробуждения)
   
    {
        std::cout << "[Test3] stress test...\n";

        FutexConditionVariable cv;
        std::mutex m;

        const int threads = std::max(2u, std::thread::hardware_concurrency());
        const int rounds = 2000; 

        std::atomic<int> arrived{0};     
        std::atomic<int> passed{0};       
        int phase = 0;                   

        std::vector<std::thread> th;
        th.reserve(threads);

        for (int i = 0; i < threads; ++i) {
            th.emplace_back([&, i] {
                for (int r = 0; r < rounds; ++r) {
                    std::unique_lock<std::mutex> lock(m);

                    arrived.fetch_add(1, std::memory_order_relaxed);

                    const int myPhase = phase; 

                    cv.wait(lock, [&] { return phase != myPhase; });

                    
                    passed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (int r = 0; r < rounds; ++r) {
      
            while (arrived.load(std::memory_order_relaxed) < (r + 1) * threads) {
                std::this_thread::yield();
            }

            {
                std::lock_guard<std::mutex> lock(m);
                phase++; 
            }
            cv.notify_all(); 
        }

        for (auto& t : th) t.join();
        std::cout << "  passed=" << passed.load() << " (expected " << threads * rounds << ")\n";
    }

    std::cout << "Done.\n";
    return 0;
}
