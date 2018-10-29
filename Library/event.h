#pragma once

#if __has_include(<mach/semaphore.h>)
#include <mach/semaphore.h>
#define __TASK_EVENT_IMPL_MACK_SEM
namespace task::__detail {
    struct __notification_impl {
        void __init() noexcept;
        
        void __deinit() noexcept;
        
        void __wait() noexcept { semaphore_wait(__sem); }

        void __signal_one() noexcept { semaphore_signal(__sem); }
        
        void __signal_all() noexcept { semaphore_signal_all(__sem); }

        semaphore_t __sem;
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

namespace task {
    class notification {
    public:
        notification() noexcept { __impl.__init(); }
        ~notification() noexcept { __impl.__deinit(); }
        void wait() noexcept { __impl.__wait(); }
        void signal_one() noexcept { __impl.__signal_one(); }
        void signal_all() noexcept { __impl.__signal_all(); }
    private:
        __detail::__notification_impl __impl;
    };
}
