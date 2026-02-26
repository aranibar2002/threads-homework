#include <vector>     
#include <functional>  
#include <thread>      
#include <algorithm>   
#include <iostream>    


template <typename T>
void ApplyFunction(
    std::vector<T>& data,                         
    const std::function<void(T&)>& transform,     
    const int threadCount = 1                     
)
{

    if (data.empty())
        return;

   
    int actualThreads = std::min(
        static_cast<int>(data.size()), 
        std::max(1, threadCount)       
    );

  
    if (actualThreads == 1)
    {
        
        for (auto& element : data)
        {
            transform(element); 
        }
        return;
    }

    
    std::vector<std::thread> threads;

   
    threads.reserve(actualThreads);

    
    size_t blockSize = data.size() / actualThreads;

    
    size_t remainder = data.size() % actualThreads;

   
    size_t beginIndex = 0;


    for (int i = 0; i < actualThreads; ++i)
    {
       
        size_t currentBlockSize = blockSize;
      
        if (i < remainder)
            ++currentBlockSize;

    
        size_t endIndex = beginIndex + currentBlockSize;

      
        threads.emplace_back(
            [&, beginIndex, endIndex]()  
            {
               
                for (size_t j = beginIndex; j < endIndex; ++j)
                {
                    transform(data[j]); 
                }
            }
        );

        
        beginIndex = endIndex;
    }

    
    for (auto& t : threads)
    {
        t.join();
    }
    
}




