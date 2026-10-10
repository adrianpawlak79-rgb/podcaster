#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <array>
#include <cstddef>

namespace zs {
// Audio does not wait, allocate or destroy. Only control threads wait for the
// last reader before replacing an object. Sequential consistency is intentional:
// a reader of the old pointer must be counted before the writer observes zero.
template<class T> class RealtimeSlot {
public:
    static_assert(std::atomic<unsigned>::is_always_lock_free, "reader count must be lock-free");
    static_assert(std::atomic<T*>::is_always_lock_free, "pointer must be lock-free");
    struct Read {
        explicit Read(RealtimeSlot& s) noexcept : slot(s) {
            slot.readers.fetch_add(1);
            ptr=slot.value.load();
        }
        ~Read() { slot.readers.fetch_sub(1); }
        Read(const Read&)=delete;
        T* get() const noexcept { return ptr; }
        T* operator->() const noexcept { return ptr; }
        explicit operator bool() const noexcept { return ptr!=nullptr; }
    private:
        RealtimeSlot& slot;
        T* ptr;
    };
    void set(T* p) { // NEVER call from audio
        std::lock_guard<std::mutex> lock(control);
        value.exchange(nullptr);
        while(readers.load()!=0) std::this_thread::yield();
        value.store(p);
    }
private:
    std::atomic<T*> value{nullptr};
    std::atomic<unsigned> readers{0};
    std::mutex control;
};

// One audio producer, one GUI consumer. Full queues drop the newest event,
// never overwrite a record which the consumer may currently be reading.
template<class T, size_t N> class SpscQueue {
public:
    bool push(const T& item) noexcept {
        const size_t w=write.load(std::memory_order_relaxed), next=(w+1)%N;
        if(next==read.load(std::memory_order_acquire)) return false;
        data[w]=item;write.store(next,std::memory_order_release);return true;
    }
    bool pop(T& item) noexcept {
        const size_t r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire))return false;
        item=data[r];read.store((r+1)%N,std::memory_order_release);return true;
    }
private:
    std::array<T,N> data{};
    std::atomic<size_t> write{0},read{0};
};
}
