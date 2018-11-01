#pragma once
#include "__config.h"
#include <atomic>
#include <chrono>
#if __has_include(<mach/semaphore.h>)
#include <mach/semaphore.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include "spin_lock.h"
#define __TASK_EVENT_IMPL_MACH_SEM
namespace task::__detail {
    semaphore_t __alloc() noexcept;

    void __release(semaphore_t sem) noexcept;

    semaphore_t __alloc_semaphore_direct() noexcept {
        semaphore_t ret;
        semaphore_create(mach_task_self(), &ret, SYNC_POLICY_FIFO, 0);
        return ret;
    }

    void __release_semaphore_direct(semaphore_t sem) noexcept {
        semaphore_destroy(mach_task_self(), sem);
    }

    class semaphore_stabilizer {
    public:
        explicit semaphore_stabilizer(semaphore_t sem) noexcept: __sem(sem) {}

        void wait() noexcept {
            while (semaphore_wait(__sem) != KERN_SUCCESS) {}
        }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) noexcept {
            for(;;) {
                auto __rel_time = __abs_time-_Clock::now();
                auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(__rel_time).count();
                auto res = div(nano, NSEC_PER_SEC);
                mach_timespec_t timespec{static_cast<unsigned int>(res.quot), static_cast<clock_res_t>(res.rem)};
                auto result = semaphore_timedwait(__sem, timespec);
                if (result == KERN_SUCCESS) return true;  // Semaphore was signalled.
                if (result == KERN_OPERATION_TIMED_OUT) return false;  // Timeout.
            }
        }

        void signal_one() noexcept { semaphore_signal(__sem); }

        void signal_all() noexcept { semaphore_signal_all(__sem); }

        semaphore_t __sem;
    };
}

namespace task {
    class notification {
    public:
        notification() noexcept
                :__sem(__detail::__alloc()) { }

        ~notification() noexcept { __detail::__release(__sem.__sem); }

        void wait() noexcept { __sem.wait(); }

        void signal_one() noexcept { __sem.signal_one(); }

        void signal_all() noexcept { __sem.signal_all(); }
    private:
        __detail::semaphore_stabilizer __sem;
    };

    class notification_timed {
    public:
        notification_timed() noexcept
                :__sem(__detail::__alloc_semaphore_direct()) { }

        ~notification_timed() noexcept { __detail::__release_semaphore_direct(__sem.__sem); }

        void wait() noexcept { __sem.wait(); }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) noexcept {
            return __sem.wait_until(__abs_time);
        }

        template <class _Clock, class _Duration, class Predicate>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time, Predicate pred) {
            while (!pred() && _Clock::now()<__abs_time)
                wait_until(__abs_time);
            return pred();
        }

        template <class _Rep, class _Period>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) noexcept {
            return wait_until(std::chrono::steady_clock::now()+__rel_time);
        }

        template <class _Rep, class _Period, class Predicate>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time, Predicate pred) {
            return wait_until(std::chrono::steady_clock::now()+__rel_time, pred);
        }

        void signal_one() noexcept { __sem.signal_one(); }

        void signal_all() noexcept { __sem.signal_all(); }
    private:
        __detail::semaphore_stabilizer __sem;
    };

    class event {
    public:
        event() noexcept
                :__sem(__detail::__alloc()) { }

        ~event() noexcept { __detail::__release(__sem.__sem); }

        void wait() noexcept {
            for (;;)
                if (auto _ = __set.load(); _!=-1) {
                    if (__set.compare_exchange_strong(_, _+1)) break;
                }
                else return;
            __sem.wait();
        }

        void set() noexcept {
            auto locked = __set.exchange(-1);
            while (locked-->0)
                __sem.signal_one();
        }
    private:
        std::atomic_int __set{0};
        __detail::semaphore_stabilizer __sem;
    };

    class event_timed {
    public:
        event_timed() noexcept
                :__sem(__detail::__alloc_semaphore_direct()) { }

        ~event_timed() noexcept { __detail::__release_semaphore_direct(__sem.__sem); }

        void wait() noexcept {
            for (;;)
                if (auto _ = __set.load(); _>-1) {
                    if (__set.compare_exchange_strong(_, _+1)) break;
                }
                else return;
            __sem.wait();
        }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) noexcept {
            for (;;)
                if (auto _ = __set.load(); _>-1) {
                    if (__set.compare_exchange_strong(_, _+1)) break;
                }
                else return true;
            auto ret = __sem.wait_until(__abs_time);
            __set.fetch_sub(1);
            return ret;
        }

        template <class _Rep, class _Period>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) noexcept {
            return wait_until(std::chrono::steady_clock::now()+__rel_time);
        }

        void set() noexcept {
            auto locked = __set.exchange(-1);
            while (locked-->0)
                __sem.signal_one();
        }
    private:
        std::atomic_int __set{0};
        __detail::semaphore_stabilizer __sem;
    };
}
#elif __has_include(<Windows.h>)
#define __TASK_EVENT_IMPL_WINDOWS
#include "__support/windows.h"
namespace task::__detail {
    TASK_API HANDLE __alloc_sem() noexcept;

    TASK_API void __release_sem(HANDLE sem) noexcept;

    TASK_API void __release_sem_long(HANDLE sem, LONG last) noexcept;

    TASK_API HANDLE __alloc_event() noexcept;

    TASK_API void __release_event(HANDLE eve) noexcept;
}

namespace task {
    class notification {
    public:
        notification() noexcept
                :__sem(__detail::__alloc_sem()) { }

        ~notification() noexcept {
            signal_all(); // Clean the state
            __detail::__release_sem(__sem);
        }

        void wait() noexcept {
            ++__count;
            WaitForSingleObject(__sem, INFINITE);
        }

        void signal_one() noexcept {
            for (;;)
                if (auto _ = __count.load(); _) {
                    if (__count.compare_exchange_strong(_, _-1)) break;
                }
                else return;
            ReleaseSemaphore(__sem, 1, nullptr);
        }

        void signal_all() noexcept {
            if (const auto _ = __count.exchange(0); _) ReleaseSemaphore(__sem, _, nullptr);
        }
    private:
        std::atomic_int __count{0};
        HANDLE __sem;
    };

    // In order to optimize for speed, we will allow spurious wake ups here on edge conditions.
    // NOTE: When timed_wait expires and signal action occurs together, sem value will mess up
    class notification_timed {
    public:
        notification_timed() noexcept
                :__sem(__detail::__alloc_sem()) { }

        ~notification_timed() noexcept {
            signal_all();
            __detail::__release_sem_long(__sem, __last);
        }

        void wait() noexcept {
            ++__count;
            WaitForSingleObject(__sem, INFINITE);
            --__count;
        }

        template <class _Rep, class _Period>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) noexcept {
            ++__count;
            auto _ = WAIT_TIMEOUT
                    !=WaitForSingleObject(__sem, std::chrono::duration<std::chrono::milliseconds>(__rel_time).count());
            --__count;
            return _;
        }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) noexcept {
            return wait_for(__abs_time-_Clock::now());
        }

        template <class Predicate>
        void wait(Predicate pred) { while (!pred()) wait(); }

        template <class _Clock, class _Duration, class Predicate>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time, Predicate pred) {
            while (!pred() && _Clock::now()<__abs_time)
                wait_until(__abs_time);
            return pred();
        }

        template <class _Rep, class _Period, class Predicate>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time, Predicate pred) {
            return wait_until(std::chrono::steady_clock::now()+__rel_time, pred);
        }

        void signal_one() noexcept {
            if (__count.load()) ReleaseSemaphore(__sem, 1, &__last);
        }

        void signal_all() noexcept {
            if (const auto _ = __count.load(); _) ReleaseSemaphore(__sem, _, &__last);
        }
    private:
        std::atomic_int __count{0};
        HANDLE __sem;
        LONG __last{0};
    };

    class event {
    public:
        event() noexcept
                :__eve(__detail::__alloc_event()) { }

        ~event() noexcept {
            if (__set)
                ResetEvent(__eve);
            __detail::__release_event(__eve);
        }

        void wait() noexcept { if (!__set) WaitForSingleObject(__eve, INFINITE); }

        void set() noexcept {
            __set = true;
            SetEvent(__eve);
        }
    private:
        std::atomic_bool __set{false};
        HANDLE __eve;
    };

    class event_timed {
    public:
        event_timed() noexcept
                :__eve(__detail::__alloc_event()) { }

        ~event_timed() noexcept {
            if (__set)
                ResetEvent(__eve);
            __detail::__release_event(__eve);
        }

        void wait() noexcept { if (!__set) WaitForSingleObject(__eve, INFINITE); }

        template <class _Rep, class _Period>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) noexcept {
            return !__set ? (WAIT_TIMEOUT
                    !=WaitForSingleObject(__eve, std::chrono::duration<std::chrono::milliseconds>(__rel_time).count()))
                    : true;
        }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) noexcept {
            return wait_for(__abs_time-_Clock::now());
        }

        void set() noexcept {
            __set = true;
            SetEvent(__eve);
        }
    private:
        std::atomic_bool __set{false};
        HANDLE __eve;
    };
}
#else
#include <mutex>
#include <condition_variable>
namespace task::__detail {
    struct __notification_impl {
        void __init() noexcept {}
        void __deinit() noexcept{}
        void __wait() noexcept {
            std::unique_lock<std::mutex> lk(_m);
            _v.wait(lk);
        }

        void __signal_one() noexcept {
            std::unique_lock<std::mutex> lk(_m);
            _v.notify_one();
        }

        void __signal_all() noexcept {
            std::unique_lock<std::mutex> lk(_m);
            _v.notify_all();
        }
        std::mutex _m;
        std::condition_variable _v;
    };
}
#endif
