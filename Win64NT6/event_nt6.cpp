//
// Created by ����˳ on 2018/10/29.
//
#include "task/event.h"
#include "task/spin_lock.h"
#ifdef __TASK_EVENT_IMPL_WINDOWS
#include "__support/windows.h"
namespace task::__detail {
    namespace {
        HANDLE* sem_pool = nullptr;
        int sem_pool_count = 0, sem_pool_current = 0;
        spin_lock sem_pool_lock;
        
        HANDLE* eve_pool = nullptr;
        int eve_pool_count = 0, eve_pool_current = 0;
        spin_lock eve_pool_lock;
    }

    HANDLE __alloc_sem() noexcept {
        sem_pool_lock.enter();
        if (sem_pool_current==sem_pool_count) {
            sem_pool_count += 16;
            sem_pool = reinterpret_cast<HANDLE*>(realloc(sem_pool, sem_pool_count*sizeof(HANDLE)));
            for (auto i = sem_pool_current; i<sem_pool_count; i++)
                sem_pool[i] = CreateSemaphore(nullptr, 0, MAXLONG, nullptr);
        }
        const auto sem = sem_pool[sem_pool_current++];
        sem_pool_lock.leave();
        return sem;
    }

    void __release_sem(const HANDLE sem) noexcept {
        sem_pool_lock.enter();
        sem_pool[--sem_pool_current] = sem;
        sem_pool_lock.leave();
    }

    void __release_sem_long(HANDLE sem, LONG last) noexcept {
        if (last != 0) {
            CloseHandle(sem);
            sem = CreateSemaphore(nullptr, 0, MAXLONG, nullptr);
        }  
        __release_sem(sem);
    }

    HANDLE __alloc_event() noexcept {
        eve_pool_lock.enter();
        if (eve_pool_current==eve_pool_count) {
            eve_pool_count += 16;
            eve_pool = reinterpret_cast<HANDLE*>(realloc(eve_pool, eve_pool_count*sizeof(HANDLE)));
            for (auto i = eve_pool_current; i<eve_pool_count; i++)
                eve_pool[i] = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        }
        const auto eve = eve_pool[eve_pool_current++];
        eve_pool_lock.leave();
        return eve;
    }

    void __release_event(const HANDLE eve) noexcept {
        eve_pool_lock.enter();
        eve_pool[--eve_pool_current] = eve;
        eve_pool_lock.leave();
    }
}
#endif
