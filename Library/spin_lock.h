#pragma once

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
#define MAX_WAIT_ITERS 8000

namespace task {
    class spin_lock {
    public:
        void enter() noexcept { while (Locked.exchange(true, std::memory_order_acquire)) WaitUntilLockIsFree(); }

        void leave() noexcept { Locked.store(false, std::memory_order_release); }
    private:
        void WaitUntilLockIsFree() const noexcept {
            size_t iters = 0;
            while (Locked.load(std::memory_order_relaxed)) {
                if (iters++<MAX_WAIT_ITERS)
                    IDLE;
                else
                    std::this_thread::yield();
            }
        }
        alignas(64) std::atomic_bool Locked = {false};
    };
}
