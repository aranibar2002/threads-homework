

#include <gtest/gtest.h>      

#include "apply_function.cpp"

#include <vector>
#include <functional>         
#include <atomic>             
#include <numeric>            



// TEST 1
// Пустой вектор — функция не должна ничего делать
// Покрывает ветку: if (data.empty())

TEST(ApplyFunctionTest, EmptyVectorDoesNothing)
{
    std::vector<int> data; 

    std::function<void(int&)> transform =
        [](int& x)
        {
            x = 999; 
        };

    ApplyFunction<int>(data, transform, 4);

    EXPECT_TRUE(data.empty());
}



// TEST 2
// Однопоточный режим
// Покрывает ветку actualThreads == 1

TEST(ApplyFunctionTest, SingleThreadExecution)
{
    std::vector<int> data = {1,2,3,4,5};

    std::function<void(int&)> transform =
        [](int& x)
        {
            x += 1;
        };

    ApplyFunction<int>(data, transform, 1);

    EXPECT_EQ(data[0], 2);
    EXPECT_EQ(data[1], 3);
    EXPECT_EQ(data[2], 4);
    EXPECT_EQ(data[3], 5);
    EXPECT_EQ(data[4], 6);
}



// TEST 3
// Потоков больше, чем элементов
// Проверяем ограничение числа потоков

TEST(ApplyFunctionTest, ThreadsMoreThanElements)
{
    std::vector<int> data = {1,2,3};

    std::function<void(int&)> transform =
        [](int& x)
        {
            x *= 2;
        };

   
    ApplyFunction<int>(data, transform, 100);

   
    EXPECT_EQ(data[0], 2);
    EXPECT_EQ(data[1], 4);
    EXPECT_EQ(data[2], 6);
}



// TEST 4
// Корректность многопоточной обработки

TEST(ApplyFunctionTest, MultiThreadCorrectness)
{
    const int N = 10000;

    std::vector<int> data(N, 1);

    std::function<void(int&)> transform =
        [](int& x)
        {
            x += 5;
        };

    ApplyFunction<int>(data, transform, 8);

    for (int v : data)
    {
        EXPECT_EQ(v, 6);
    }
}



// TEST 5
// Проверка корректного распределения remainder

TEST(ApplyFunctionTest, UnevenBlockDistribution)
{
    std::vector<int> data(10, 0);

    std::function<void(int&)> transform =
        [](int& x)
        {
            x++;
        };

    ApplyFunction<int>(data, transform, 3);

    for (int v : data)
    {
        EXPECT_EQ(v, 1);
    }
}



// TEST 6
// Проверяем, что transform вызывается ровно N раз
// Используем atomic (потокобезопасный счётчик)

TEST(ApplyFunctionTest, EveryElementProcessedExactlyOnce)
{
    const int N = 5000;

    std::vector<int> data(N, 0);

    std::atomic<int> counter{0}; 

    std::function<void(int&)> transform =
        [&](int& x)
        {
            x = 1;
            counter.fetch_add(1); 
        };

    ApplyFunction<int>(data, transform, 6);

    EXPECT_EQ(counter.load(), N);
}



// TEST 7
// Проверяем шаблонность (другой тип)

TEST(ApplyFunctionTest, WorksWithDoubleType)
{
    std::vector<double> data = {1.5, 2.5, 3.5};

    std::function<void(double&)> transform =
        [](double& x)
        {
            x *= 2.0;
        };

    ApplyFunction<double>(data, transform, 2);

    EXPECT_DOUBLE_EQ(data[0], 3.0);
    EXPECT_DOUBLE_EQ(data[1], 5.0);
    EXPECT_DOUBLE_EQ(data[2], 7.0);
}



// TEST 8
// Стресс-тест большого массива

TEST(ApplyFunctionTest, LargeDataStress)
{
    const int N = 200000;

    std::vector<int> data(N);

    std::iota(data.begin(), data.end(), 0);

    std::function<void(int&)> transform =
        [](int& x)
        {
            x = x * x;
        };

    ApplyFunction<int>(data, transform, 12);

   
    for (int i = 0; i < N; ++i)
    {
        EXPECT_EQ(data[i], i * i);
    }
}


// TEST 9
// Отрицательное число потоков
// Проверяет ветку std::max(1, threadCount)


TEST(ApplyFunctionTest, NegativeThreadCount)
{
    std::vector<int> data = {1,2,3};

    std::function<void(int&)> transform =
        [](int& x)
        {
            x++;
        };

  
    ApplyFunction<int>(data, transform, -5);

    EXPECT_EQ(data[0], 2);
    EXPECT_EQ(data[1], 3);
    EXPECT_EQ(data[2], 4);
}


// TEST 10
// Число потоков равно числу элементов
// Проверяется отдельный путь разбиения блоков

TEST(ApplyFunctionTest, ThreadsEqualElements)
{
    std::vector<int> data = {1,2,3,4};

    std::function<void(int&)> transform =
        [](int& x)
        {
            x *= 3;
        };

    ApplyFunction<int>(data, transform, 4);

    EXPECT_EQ(data[0], 3);
    EXPECT_EQ(data[1], 6);
    EXPECT_EQ(data[2], 9);
    EXPECT_EQ(data[3], 12);
}

// TEST 11
// Идеальное деление без remainder
// remainder == 0

TEST(ApplyFunctionTest, PerfectBlockSplit)
{
    std::vector<int> data(12, 1);

    std::function<void(int&)> transform =
        [](int& x)
        {
            x++;
        };

    ApplyFunction<int>(data, transform, 4);

    for (int v : data)
    {
        EXPECT_EQ(v, 2);
    }
}

// TEST 12
// Маленький remainder (малый массив)
// gcov часто считает это отдельной веткой

TEST(ApplyFunctionTest, SmallRemainderCase)
{
    std::vector<int> data(5, 10);

    std::function<void(int&)> transform =
        [](int& x)
        {
            x -= 1;
        };

    ApplyFunction<int>(data, transform, 3);

    for (int v : data)
    {
        EXPECT_EQ(v, 9);
    }
}


// TEST 13
// Вектор из одного элемента

TEST(ApplyFunctionTest, SingleElementVector)
{
    std::vector<int> data = {42};

    std::function<void(int&)> transform =
        [](int& x)
        {
            x = 0;
        };


    ApplyFunction<int>(data, transform, 8);

    EXPECT_EQ(data[0], 0);
}



// TEST 14
// Проверка работы transform с внешним состоянием
// Покрывает выполнение lambda внутри потоков

#include <mutex>

TEST(ApplyFunctionTest, ExternalStateModification)
{
    std::vector<int> data(100, 1);

    int sum = 0;          
    std::mutex m;         

    std::function<void(int&)> transform =
        [&](int& x)
        {
            
            std::lock_guard<std::mutex> lock(m);

            
            sum += x;
        };

    ApplyFunction<int>(data, transform, 5);

    
    EXPECT_EQ(sum, 100);
}



// TEST 15
// threadCount == 0
// отдельная ветка std::max(1, threadCount)

TEST(ApplyFunctionTest, ZeroThreadCount)
{
    std::vector<int> data = {5,5,5};

    std::function<void(int&)> transform =
        [](int& x)
        {
            x += 10;
        };

    
    ApplyFunction<int>(data, transform, 0);

    EXPECT_EQ(data[0], 15);
    EXPECT_EQ(data[1], 15);
    EXPECT_EQ(data[2], 15);
}
