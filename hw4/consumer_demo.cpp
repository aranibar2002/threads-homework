#include "ipc_mpsc_queue.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

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
    if (argc != 5) {
        std::cerr
            << "Usage: " << argv[0]
            << " <shm_name> <total_bytes> <desired_type> <target_count>\n";
        return 1;
    }

    const std::string shm_name = argv[1];
    const std::size_t total_bytes = static_cast<std::size_t>(std::stoull(argv[2]));
    const std::uint32_t desired_type = static_cast<std::uint32_t>(std::stoul(argv[3]));
    const int target_count = std::stoi(argv[4]);

    try {
        ConsumerNode consumer(shm_name, total_bytes);

        int received = 0;

        while (received < target_count) {
            ReceivedMessage msg;

            if (!consumer.TryReceiveFiltered(desired_type, msg)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            if (msg.type == kTextMessage) {
                if (msg.length != sizeof(TextMessage)) {
                    std::cerr << "bad TEXT message length\n";
                    continue;
                }

                TextMessage text_msg;
                std::memcpy(&text_msg, msg.payload.data(), sizeof(TextMessage));

                std::cout << "[consumer] got TEXT from producer "
                          << text_msg.producer_id
                          << ", index=" << text_msg.index
                          << ", text=\"" << text_msg.text << "\"\n";
            } else if (msg.type == kNumberMessage) {
                if (msg.length != sizeof(NumberMessage)) {
                    std::cerr << "bad NUMBER message length\n";
                    continue;
                }

                NumberMessage num_msg;
                std::memcpy(&num_msg, msg.payload.data(), sizeof(NumberMessage));

                std::cout << "[consumer] got NUMBER from producer "
                          << num_msg.producer_id
                          << ", index=" << num_msg.index
                          << ", value=" << num_msg.value << "\n";
            } else {
                std::cout << "[consumer] got message of unknown type " << msg.type << "\n";
            }

            ++received;
        }

        std::cout << "[consumer] finished, received " << received
                  << " messages of type " << desired_type << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "consumer error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
