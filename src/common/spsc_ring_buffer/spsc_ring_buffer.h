#ifndef TIBIA_COMMON_SPSC_RING_BUFFER_H_
#define TIBIA_COMMON_SPSC_RING_BUFFER_H_ 1

#include <atomic>
#include <cstdint>
#include <cstring>

// Lock-free single-producer, single-consumer ring buffer.
// Size must be a power of two. Usable capacity is (Size - 1) bytes.
// Producer calls write() from one thread, consumer calls read() from another.
template <unsigned int Size>
class SPSCRingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of two");
    static_assert(Size >= 2, "Size must be at least 2");

public:
    SPSCRingBuffer() : write_pos_(0), read_pos_(0) {}

    // Write `len` bytes into the buffer. Returns true on success, false if
    // there is not enough space (no partial writes — all or nothing).
    bool write(const uint8_t *data, int len) {
        if (len <= 0) return len == 0;
        unsigned int w = write_pos_.load(std::memory_order_relaxed);
        unsigned int r = read_pos_.load(std::memory_order_acquire);
        unsigned int free = free_space(w, r);
        if ((unsigned int)len > free) return false;

        unsigned int first = Size - (w & MASK_);
        if (first >= (unsigned int)len) {
            memcpy(&buffer_[w & MASK_], data, len);
        } else {
            memcpy(&buffer_[w & MASK_], data, first);
            memcpy(&buffer_[0], data + first, len - first);
        }

        write_pos_.store(w + (unsigned int)len, std::memory_order_release);
        return true;
    }

    // Read up to `max_len` bytes from the buffer. Returns number of bytes
    // actually read (0 if buffer is empty).
    int read(uint8_t *data, int max_len) {
        if (max_len <= 0) return 0;
        unsigned int r = read_pos_.load(std::memory_order_relaxed);
        unsigned int w = write_pos_.load(std::memory_order_acquire);
        unsigned int avail = w - r;
        if (avail == 0) return 0;
        if (avail > (unsigned int)max_len) avail = (unsigned int)max_len;

        unsigned int first = Size - (r & MASK_);
        if (first >= avail) {
            memcpy(data, &buffer_[r & MASK_], avail);
        } else {
            memcpy(data, &buffer_[r & MASK_], first);
            memcpy(data + first, &buffer_[0], avail - first);
        }

        read_pos_.store(r + avail, std::memory_order_release);
        return (int)avail;
    }

    int available_bytes() const {
        unsigned int w = write_pos_.load(std::memory_order_acquire);
        unsigned int r = read_pos_.load(std::memory_order_acquire);
        return (int)(w - r);
    }

private:
    static constexpr unsigned int MASK_ = Size - 1;

    unsigned int free_space(unsigned int w, unsigned int r) const {
        return (Size - 1) - (w - r);
    }

    uint8_t buffer_[Size];
    std::atomic<unsigned int> write_pos_;
    std::atomic<unsigned int> read_pos_;
};

#endif // TIBIA_COMMON_SPSC_RING_BUFFER_H_
