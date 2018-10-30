//
// Created by 刘予顺 on 2018/10/29.
//
#include "event.h"
#ifdef __TASK_EVENT_IMPL_MACH_SEM
#include <cstdlib>
#include <mach/mach.h>
#include "spin_lock.h"
namespace task::__detail {
    semaphore_t* sem_pool = nullptr;
    int sem_pool_count = 0;
    int sem_pool_current = 0;
    spin_lock sem_pool_lock;

    semaphore_t new_sem_from_pool() noexcept {
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

    void restore_sem_to_pool(semaphore_t sem) noexcept {
        sem_pool_lock.enter();
        sem_pool[--sem_pool_current] = sem;
        sem_pool_lock.leave();
    }

    __notification_impl::__notification_impl() noexcept
            :__sem(new_sem_from_pool()) { }

    __notification_impl::~__notification_impl() noexcept { restore_sem_to_pool(__sem); }
}
#endif
