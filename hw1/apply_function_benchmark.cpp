

#include <benchmark/benchmark.h>

#include "apply_function.cpp"   

#include <vector>
#include <functional>
#include <cmath>
#include <numeric>



// SCENARIO 1
// Лёгкая операция и многопоточность медленнее

static void BM_SingleThread_LightWork(benchmark::State& state)
{
    
    const int N = state.range(0);

  
    std::vector<int> data(N, 1);

   
    std::function<void(int&)> transform =
        [](int& x)
        {
            x += 1; 
        };

    
    for (auto _ : state)
    {

        state.PauseTiming();

        std::fill(data.begin(), data.end(), 1);

        state.ResumeTiming();

        ApplyFunction<int>(data, transform, 1);
    }
}


// Та же лёгкая операция, но уже много потоков

static void BM_MultiThread_LightWork(benchmark::State& state)
{
    const int N = state.range(0);

    std::vector<int> data(N, 1);

    std::function<void(int&)> transform =
        [](int& x)
        {
            x += 1;
        };

    for (auto _ : state)
    {
        state.PauseTiming();
        std::fill(data.begin(), data.end(), 1);
        state.ResumeTiming();

       
        ApplyFunction<int>(data, transform, 8);
    }
}


// SCENARIO 2
// тяжелая операция и многопоточность быстрее


static void BM_SingleThread_HeavyWork(benchmark::State& state)
{
    const int N = state.range(0);

    std::vector<double> data(N, 1.001);

    std::function<void(double&)> transform =
        [](double& x)
        {
            for (int i = 0; i < 100; ++i)
            {
                x = std::sin(x);
                x = std::cos(x);
                x = std::sqrt(std::abs(x) + 1.0);
            }
        };

    for (auto _ : state)
    {
        state.PauseTiming();
        std::fill(data.begin(), data.end(), 1.001);
        state.ResumeTiming();

        ApplyFunction<double>(data, transform, 1);
    }
}



// Многопоточная тяжёлая работа

static void BM_MultiThread_HeavyWork(benchmark::State& state)
{
    const int N = state.range(0);

    std::vector<double> data(N, 1.001);

    std::function<void(double&)> transform =
        [](double& x)
        {
            for (int i = 0; i < 100; ++i)
            {
                x = std::sin(x);
                x = std::cos(x);
                x = std::sqrt(std::abs(x) + 1.0);
            }
        };

    for (auto _ : state)
    {
        state.PauseTiming();
        std::fill(data.begin(), data.end(), 1.001);
        state.ResumeTiming();

        ApplyFunction<double>(data, transform, 8);
    }
}


//benchmark

BENCHMARK(BM_SingleThread_LightWork)
    ->Arg(1000)
    ->Arg(100000);

BENCHMARK(BM_MultiThread_LightWork)
    ->Arg(1000)
    ->Arg(100000);

BENCHMARK(BM_SingleThread_HeavyWork)
    ->Arg(1000)
    ->Arg(20000);

BENCHMARK(BM_MultiThread_HeavyWork)
    ->Arg(1000)
    ->Arg(20000);



// Точка входа benchmark

BENCHMARK_MAIN();
