#include "ipc_mpsc_queue.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

using namespace ipc_mpsc;

enum DemoMessageType : std::uint32_t {
    kTextMessage = 1,
    kNumberMessage = 2
};

struct TextMessage {
    int producer_id = 0;
    int index = 0;
    char text[64]{};
};

struct NumberMessage {
    int producer_id = 0;
    int index = 0;
    std::uint64_t value = 0;
};

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr
            << "Usage: " << argv[0]
            << " <shm_name> <total_bytes> <max_payload> <producer_id> <message_count>\n";
        return 1;
    }

    const std::string shm_name = argv[1];
    const std::size_t total_bytes = static_cast<std::size_t>(std::stoull(argv[2]));
    const std::size_t max_payload = static_cast<std::size_t>(std::stoull(argv[3]));
    const int producer_id = std::stoi(argv[4]);
    const int message_count = std::stoi(argv[5]);

    try {
        ProducerNode producer(shm_name, total_bytes, max_payload);

        for (int i = 0; i < message_count; ++i) {
            if (i % 2 == 0) {
                TextMessage msg;
                msg.producer_id = producer_id;
                msg.index = i;
                std::snprintf(msg.text, sizeof(msg.text),
                              "hello from producer %d, msg %d",
                              producer_id, i);

                while (!producer.TrySendObject(kTextMessage, msg)) {
                    std::this_thread::yield();
                }

                std::cout << "[producer " << producer_id << "] sent TEXT #" << i
                          << ": " << msg.text << "\n";
            } else {
                NumberMessage msg;
                msg.producer_id = producer_id;
                msg.index = i;
                msg.value = static_cast<std::uint64_t>(producer_id) * 1000000ULL + i;

                while (!producer.TrySendObject(kNumberMessage, msg)) {
                    std::this_thread::yield();
                }

                std::cout << "[producer " << producer_id << "] sent NUMBER #" << i
                          << ": value=" << msg.value << "\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        std::cout << "[producer " << producer_id << "] finished\n";
    } catch (const std::exception& ex) {
        std::cerr << "producer error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
