#pragma once

#include <cstddef>
#include "__config.h"

#if __has_include(<dispatch/dispatch.h>)
#define __TASK_TASK_IMPL_GCD
#elif __has_include(<concrt.h>)
#define __TASK_TASK_IMPL_CCRT
#else
#define __TASK_TASK_IMPL_GENERIC
#endif

namespace task {
    enum class priority : int {
        background = 0, utility = 1, normal = 2, interactive = 3
    };
    class task {
    public:
        virtual ~task() = default;
        virtual void fire() noexcept = 0;
        virtual bool reuse() const noexcept { return false; }
#ifdef __TASK_TASK_IMPL_GENERIC
    private:
        friend class TaskQueue;
        friend class Worker;
        task* __next = nullptr;
#endif
    };

    TASK_API void apply_one(task* __task, size_t __iterations, priority __priority) noexcept;

    TASK_API void enqueue_one(task* __task, priority __priority) noexcept;

    TASK_API priority get_current_thread_priority() noexcept;
}
