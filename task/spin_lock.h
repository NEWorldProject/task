#pragma once
#ifndef NEWORLD_SYNC_PMR
#define NEWORLD_SYNC_PMR
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define IDLE _mm_pause()
#elif __has_include(<intrin.h>)
#include <intrin.h>
#define IDLE _mm_pause()
#else
#define IDLE
#endif

#include <thread>
#include <atomic>
#define MAX_WAIT_ITERS 8000

namespace task {
    class spin_lock {
    public:
        void enter() noexcept { while (__lock.exchange(true, std::memory_order_acquire)) waitUntilLockIsFree(); }

        void leave() noexcept { __lock.store(false, std::memory_order_release); }
    private:
        void waitUntilLockIsFree() const noexcept {
            size_t iterations = 0;
            while (__lock.load(std::memory_order_relaxed)) {
                if (iterations++<MAX_WAIT_ITERS)
                    IDLE;
                else
                    std::this_thread::yield();
            }
        }
        alignas(64) std::atomic_bool __lock = {false};
    };
}
#endif
