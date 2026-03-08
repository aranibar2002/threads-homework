#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <thread>
#include <chrono>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ipc_mpsc {

inline constexpr std::uint64_t kMagic = 0x4D50534351554555ULL;
inline constexpr std::uint32_t kProtocolVersion = 1;
inline constexpr std::size_t kCacheLine = 64;

inline std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

inline std::size_t RoundDownToPowerOfTwo(std::size_t value) {
    if (value < 2) {
        return 0;
    }
    std::size_t power = 1;
    while ((power << 1) <= value) {
        power <<= 1;
    }
    return power;
}

inline void ValidateShmName(std::string_view name) {
    if (name.empty() || name.front() != '/') {
        throw std::runtime_error("shared memory name must start with '/'");
    }
}

struct MessageHeader {
    std::uint32_t type = 0;
    std::uint32_t length = 0;
};

struct alignas(kCacheLine) CellPrefix {
    std::atomic<std::uint64_t> sequence{0};
    MessageHeader header{};
};

struct alignas(kCacheLine) SharedQueueHeader {
    std::uint64_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t capacity = 0;
    std::uint32_t max_payload = 0;
    std::uint32_t cell_size = 0;
    std::uint32_t reserved = 0;
    alignas(kCacheLine) std::atomic<std::uint64_t> enqueue_pos{0};
    alignas(kCacheLine) std::atomic<std::uint64_t> dequeue_pos{0};
};

class SharedMemoryRegion {
public:
    SharedMemoryRegion() = default;

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;

    SharedMemoryRegion(SharedMemoryRegion&& other) noexcept {
        MoveFrom(std::move(other));
    }

    SharedMemoryRegion& operator=(SharedMemoryRegion&& other) noexcept {
        if (this != &other) {
            Reset();
            MoveFrom(std::move(other));
        }
        return *this;
    }

    ~SharedMemoryRegion() {
        Reset();
    }

    void OpenOrCreate(const std::string& name, std::size_t size, bool create) {
        ValidateShmName(name);

        int flags = O_RDWR;
        if (create) {
            flags |= O_CREAT | O_EXCL;
        }

        int fd = shm_open(name.c_str(), flags, 0666);
        if (fd == -1) {
            throw std::runtime_error("shm_open failed");
        }

        if (create) {
            if (ftruncate(fd, static_cast<off_t>(size)) == -1) {
                close(fd);
                throw std::runtime_error("ftruncate failed");
            }
        }

        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("mmap failed");
        }

        fd_ = fd;
        ptr_ = ptr;
        size_ = size;
    }

    void OpenExisting(const std::string& name, std::size_t size) {
        OpenOrCreate(name, size, false);
    }

    void Reset() {
        if (ptr_ != nullptr) {
            munmap(ptr_, size_);
            ptr_ = nullptr;
        }
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
        size_ = 0;
    }

    void* Data() const {
        return ptr_;
    }

    std::size_t Size() const {
        return size_;
    }

    static void Unlink(const std::string& name) {
        ValidateShmName(name);
        shm_unlink(name.c_str());
    }

private:
    void MoveFrom(SharedMemoryRegion&& other) noexcept {
        fd_ = other.fd_;
        ptr_ = other.ptr_;
        size_ = other.size_;
        other.fd_ = -1;
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    int fd_ = -1;
    void* ptr_ = nullptr;
    std::size_t size_ = 0;
};

struct ReceivedMessage {
    std::uint32_t type = 0;
    std::uint32_t length = 0;
    std::string payload;
};

class QueueView {
protected:
    QueueView() = default;

    void BindPointers() {
        header_ = reinterpret_cast<SharedQueueHeader*>(region_.Data());
        header_bytes_ = AlignUp(sizeof(SharedQueueHeader), kCacheLine);

        for (int attempt = 0; attempt < 200; ++attempt) {
            if (header_->magic == kMagic) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (header_->magic != kMagic) {
            throw std::runtime_error("shared memory magic mismatch");
        }

        if (header_->version != kProtocolVersion) {
            throw std::runtime_error("protocol version mismatch");
        }

        if (header_->total_bytes != region_.Size()) {
            throw std::runtime_error("shared memory size mismatch");
        }

        cells_base_ = static_cast<std::byte*>(region_.Data()) + header_bytes_;
    }

    CellPrefix* CellAt(std::size_t index) const {
        std::byte* cell_ptr = cells_base_ + index * header_->cell_size;
        return reinterpret_cast<CellPrefix*>(cell_ptr);
    }

    std::byte* PayloadAt(std::size_t index) const {
        CellPrefix* prefix = CellAt(index);
        return reinterpret_cast<std::byte*>(prefix) + sizeof(CellPrefix);
    }

protected:
    SharedMemoryRegion region_{};
    SharedQueueHeader* header_ = nullptr;
    std::byte* cells_base_ = nullptr;
    std::size_t header_bytes_ = 0;
};

class ProducerNode : public QueueView {
public:
    ProducerNode(const std::string& shm_name, std::size_t total_bytes, std::size_t max_payload)
        : shm_name_(shm_name) {
        ValidateShmName(shm_name_);

        bool created = false;
        try {
            region_.OpenOrCreate(shm_name_, total_bytes, true);
            created = true;
        } catch (...) {
            region_.OpenExisting(shm_name_, total_bytes);
        }

        if (created) {
            InitializeFreshRegion(total_bytes, max_payload);
        }

        BindPointers();
    }

    static void Destroy(const std::string& shm_name) {
        SharedMemoryRegion::Unlink(shm_name);
    }

    bool TrySend(std::uint32_t type, const void* data, std::uint32_t length) {
        if (length > header_->max_payload) {
            throw std::runtime_error("message too large for queue max_payload");
        }

        std::uint64_t pos = header_->enqueue_pos.load(std::memory_order_relaxed);

        for (;;) {
            CellPrefix* cell = CellAt(static_cast<std::size_t>(pos & (header_->capacity - 1)));
            std::uint64_t seq = cell->sequence.load(std::memory_order_acquire);
            std::int64_t dif = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos);

            if (dif == 0) {
                if (header_->enqueue_pos.compare_exchange_weak(
                        pos,
                        pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break;
                }
                continue;
            }

            if (dif < 0) {
                return false;
            }

            pos = header_->enqueue_pos.load(std::memory_order_relaxed);
        }

        std::size_t index = static_cast<std::size_t>(pos & (header_->capacity - 1));
        CellPrefix* cell = CellAt(index);
        std::byte* payload = PayloadAt(index);

        cell->header.type = type;
        cell->header.length = length;

        if (length > 0) {
            std::memcpy(payload, data, length);
        }

        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    template <class T>
    bool TrySendObject(std::uint32_t type, const T& object) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "TrySendObject requires trivially copyable type");
        return TrySend(type, &object, static_cast<std::uint32_t>(sizeof(T)));
    }

private:
    void InitializeFreshRegion(std::size_t total_bytes, std::size_t max_payload) {
        std::memset(region_.Data(), 0, region_.Size());

        std::size_t header_bytes = AlignUp(sizeof(SharedQueueHeader), kCacheLine);
        std::size_t cell_size = AlignUp(sizeof(CellPrefix) + max_payload, kCacheLine);

        if (total_bytes <= header_bytes + cell_size) {
            throw std::runtime_error("shared memory too small for queue");
        }

        std::size_t raw_capacity = (total_bytes - header_bytes) / cell_size;
        std::size_t capacity = RoundDownToPowerOfTwo(raw_capacity);

        if (capacity < 2) {
            throw std::runtime_error("queue capacity is too small");
        }

        auto* header = new (region_.Data()) SharedQueueHeader();
        header->version = kProtocolVersion;
        header->total_bytes = static_cast<std::uint32_t>(total_bytes);
        header->capacity = static_cast<std::uint32_t>(capacity);
        header->max_payload = static_cast<std::uint32_t>(max_payload);
        header->cell_size = static_cast<std::uint32_t>(cell_size);
        header->enqueue_pos.store(0, std::memory_order_relaxed);
        header->dequeue_pos.store(0, std::memory_order_relaxed);

        std::byte* base = static_cast<std::byte*>(region_.Data()) + header_bytes;
        for (std::size_t i = 0; i < capacity; ++i) {
            std::byte* cell_ptr = base + i * cell_size;
            auto* cell = new (cell_ptr) CellPrefix();
            cell->sequence.store(i, std::memory_order_relaxed);
            cell->header.type = 0;
            cell->header.length = 0;
        }

        header->magic = kMagic;
    }

private:
    std::string shm_name_;
};

class ConsumerNode : public QueueView {
public:
    ConsumerNode(const std::string& shm_name, std::size_t total_bytes) {
        ValidateShmName(shm_name);

        bool opened = false;
        for (int attempt = 0; attempt < 200; ++attempt) {
            try {
                region_.OpenExisting(shm_name, total_bytes);
                opened = true;
                break;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if (!opened) {
            throw std::runtime_error("failed to open existing shared memory");
        }

        BindPointers();
    }

    bool TryReceiveAny(ReceivedMessage& out) {
        std::uint64_t pos = header_->dequeue_pos.load(std::memory_order_relaxed);

        std::size_t index = static_cast<std::size_t>(pos & (header_->capacity - 1));
        CellPrefix* cell = CellAt(index);

        std::uint64_t seq = cell->sequence.load(std::memory_order_acquire);
        std::int64_t dif = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos + 1);

        if (dif < 0) {
            return false;
        }

        if (dif > 0) {
            return false;
        }

        const std::uint32_t type = cell->header.type;
        const std::uint32_t length = cell->header.length;

        if (length > header_->max_payload) {
            throw std::runtime_error("corrupted message length");
        }

        out.type = type;
        out.length = length;
        out.payload.assign(reinterpret_cast<char*>(PayloadAt(index)), length);

        header_->dequeue_pos.store(pos + 1, std::memory_order_relaxed);
        cell->sequence.store(pos + header_->capacity, std::memory_order_release);

        return true;
    }

    bool TryReceiveFiltered(std::uint32_t desired_type, ReceivedMessage& out) {
        for (;;) {
            ReceivedMessage tmp;
            if (!TryReceiveAny(tmp)) {
                return false;
            }
            if (tmp.type == desired_type) {
                out = std::move(tmp);
                return true;
            }
        }
    }

    std::uint32_t MaxPayload() const {
        return header_->max_payload;
    }
};

} 
