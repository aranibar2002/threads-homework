#pragma once

#include <condition_variable>   
#include <mutex>                
#include <optional>             
#include <stdexcept>            
#include <utility>              



template <class T>
class UnbufferedChannel {
public:
    UnbufferedChannel() = default;

   
    void Send(T v) {
        std::unique_lock<std::mutex> lock(m_mutex); 

        
        if (m_closed) {
            throw std::runtime_error("send on closed channel");
        }

        m_cvSend.wait(lock, [this] {
            return m_closed || (m_receiverWaiting && !m_value.has_value());
        });

        
        if (m_closed) {
            throw std::runtime_error("send on closed channel");
        }

        m_value.emplace(std::move(v));

        m_cvRecv.notify_one();

        m_cvSend.wait(lock, [this] {
            return !m_value.has_value();
        });

        
    }

    std::optional<T> Recv() {
        std::unique_lock<std::mutex> lock(m_mutex); 

        m_receiverWaiting = true;

        m_cvSend.notify_one();

        m_cvRecv.wait(lock, [this] {
            return m_value.has_value() || m_closed;
        });

        if (!m_value.has_value()) {
            m_receiverWaiting = false;        
            return std::nullopt;              
        }

        std::optional<T> result(std::move(*m_value));

        m_value.reset();

        m_receiverWaiting = false;

        m_cvSend.notify_one();

        return result;
    }

    void Close() {
        std::lock_guard<std::mutex> lock(m_mutex); 

        if (m_closed) {
            return;
        }

        m_closed = true; 

        m_cvSend.notify_all();
        m_cvRecv.notify_all();
    }

private:
    std::mutex m_mutex;                     
    std::condition_variable m_cvSend;       
    std::condition_variable m_cvRecv;       

    std::optional<T> m_value;               
    bool m_receiverWaiting = false;         
    bool m_closed = false;                  
};
