//
// Created by 刘予顺 on 2018/10/29.
//
#include "event.h"
#ifdef __TASK_EVENT_IMPL_MACH_SEM
#include <cstdlib>
#include "spin_lock.h"
namespace task::__detail {
    namespace {
        semaphore_t* sem_pool = nullptr;
        int sem_pool_count = 0, sem_pool_current = 0;
        spin_lock sem_pool_lock;
    }

    semaphore_t __alloc() noexcept {
        semaphore_t sem;
        sem_pool_lock.enter();
        if (sem_pool_current==sem_pool_count) {
            sem_pool_count += 16;
            sem_pool = reinterpret_cast<semaphore_t*>(realloc(sem_pool, sem_pool_count*sizeof(semaphore_t)));
            for (int i = sem_pool_current; i<sem_pool_count; i++) {
                semaphore_create(mach_task_self(), &sem_pool[i], SYNC_POLICY_FIFO, 0);
            }
        }
        sem = sem_pool[sem_pool_current++];
        sem_pool_lock.leave();
        return sem;
    }

    void __release(semaphore_t sem) noexcept {
        sem_pool_lock.enter();
        sem_pool[--sem_pool_current] = sem;
        sem_pool_lock.leave();
    }
}
#endif
