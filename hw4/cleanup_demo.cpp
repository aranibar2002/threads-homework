#include "ipc_mpsc_queue.h"

#include <iostream>
#include <string>

using namespace ipc_mpsc;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <shm_name>\n";
        return 1;
    }

    try {
        ProducerNode::Destroy(argv[1]);
        std::cout << "shared memory removed: " << argv[1] << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "cleanup error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
