#include "task.h"

#ifdef __TASK_TASK_IMPL_GCD
#import <dispatch/dispatch.h>
namespace task {
    namespace {
        struct { } _PriorityTagPlaceHolder;

        struct Init {
            Init() {
                for (uintptr_t i = 0; i < 4; ++i)
                    dispatch_queue_set_specific(Queues[i], &_PriorityTagPlaceHolder, (void*)(i + 1), nullptr);
            };

            dispatch_queue_t Queues[4] = {
                    dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0),
                    dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
                    dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                    dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0)
            };
        } _Dispatch;
    }

    void __early_stop() noexcept { }

    void enqueue_one(task* __task, priority __priority) noexcept {
        dispatch_async_f(_Dispatch.Queues[static_cast<int>(__priority)], __task,
            [](void* _) { reinterpret_cast<task*>(_)->fire(); });
    }

    priority get_current_thread_priority() noexcept {
        auto _ = dispatch_get_specific(&_PriorityTagPlaceHolder);
        return _ ? priority((int)((uintptr_t)_) - 1) : priority::normal;
    }

    void apply_one(task* __task, size_t __iterations, priority __priority) noexcept {
        dispatch_apply_f(__iterations, _Dispatch.Queues[static_cast<int>(__priority)], __task,
            [](void* _, size_t) { reinterpret_cast<task*>(_)->fire(); });
    }
}
#endif
