#include "task.h"

#ifdef __TASK_TASK_IMPL_CCRT
#include <concrt.h>
#include "__support/windows.h"
namespace task {
    using namespace Concurrency;
    namespace {
        constexpr int _Priorities[4] = {
            THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL
        };

        struct Init {
            static Scheduler* Create(int pr) noexcept {
                SchedulerPolicy policy;
                policy.SetPolicyValue(SchedulerKind, UmsThreadDefault);
                policy.SetPolicyValue(ContextPriority, _Priorities[pr]);
                return Scheduler::Create(policy);
            }

            ~Init() {
                for (int i = 0; i < 4; ++i)
                    Queues[i]->Release();
            }

            Scheduler* Queues[4] = { Create(0), Create(1), Create(2), Create(3) };
        } _Dispatch;

        priority convert(int in) {
            switch (in) {
            case THREAD_PRIORITY_TIME_CRITICAL:
            case THREAD_PRIORITY_HIGHEST:
            case THREAD_PRIORITY_ABOVE_NORMAL:
                return priority::interactive;
            case THREAD_PRIORITY_NORMAL:
                return priority::normal;
            case THREAD_PRIORITY_BELOW_NORMAL:
                return priority::utility;
            case THREAD_PRIORITY_LOWEST:
            case THREAD_PRIORITY_IDLE:
                return priority::background;
            }
            return priority::normal;
        }
    }

    TASK_API void __early_stop() noexcept { }

    void enqueue_one(task* __task, priority __priority) noexcept {
        _Dispatch.Queues[static_cast<int>(__priority)]->ScheduleTask([](void* _) { reinterpret_cast<task*>(_)->fire(); }, __task);
    }

    priority get_current_thread_priority() noexcept { return convert(GetThreadPriority(GetCurrentThread())); }

    void apply_one(task* __task, size_t __iterations, priority __priority) noexcept { 
        for (int i = 0; i < __iterations; ++i)
            enqueue_one(__task, __priority);
    }
}
#endif
