#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <vector>
#include "task.h"

#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#include <array>
#define IDLE _mm_pause()
#elif __has_include(<intrin.h>)
#include <intrin.h>
#define IDLE _mm_pause()
#else
#define IDLE
#endif

#define MAX_WAIT_ITERS 50

constexpr int PriorityLevels = 32;
constexpr int hardware_destructive_interference_size = 64;

class SpinLock {
public:
    void Enter() noexcept { while (Locked.exchange(true, std::memory_order_acquire)) WaitUntilLockIsFree(); }

    void Leave() noexcept { Locked.store(false, std::memory_order_relaxed); }

private:
    void WaitUntilLockIsFree() const noexcept{
        size_t numIters = 0;
        while (Locked.load(std::memory_order_relaxed)) {
            if (numIters++ < MAX_WAIT_ITERS)
                IDLE;
            else
                std::this_thread::yield();
        }
    }
    alignas(hardware_destructive_interference_size) std::atomic_bool Locked = {false};
};

class ASyncTaskQueue {
public:
    void EnqueueOne(task* dispatchee) { _Queue.push(dispatchee); }

    task* DequeueOne() {
        task* ret = nullptr;
        if (!Empty()) {
            ret = _Queue.front();
            _Queue.pop();
        }
        return ret;
    }

    bool Empty() const noexcept { return _Queue.empty(); }
private:
    std::queue<task*> _Queue {};
};

class BasicDispatcher {
public:
    void EnqueueOne(task* dispatchee, int level) {
        _Lock.Enter();
        _DispatchQueues[level].EnqueueOne(dispatchee);
        _Lock.Leave();
    }

    task* DispatchOne() {
        task* ret = nullptr;
        _Lock.Enter();
        for(int i = PriorityLevels - 1; i > -1; --i)
            if (ret = _DispatchQueues[i].DequeueOne(), ret)
                break;
        _Lock.Leave();
        return ret;
    }
private:
    SpinLock _Lock;
    std::array<ASyncTaskQueue, PriorityLevels> _DispatchQueues;
};

class ThreadGroup {
public:
    template<class Func>
    explicit ThreadGroup(Func fn) {
        for (auto i = 0; i<std::thread::hardware_concurrency(); ++i)
            _Threads.emplace_back(fn);
    }
    ~ThreadGroup() {
        for (auto&& x : _Threads)
            if (x.joinable())
                x.join();
    }
private:
    std::vector<std::thread> _Threads;
};

class ThreadPool {
    struct : task {
        ThreadPool* _Pool = nullptr;
        void fire() noexcept override { _Pool->_Stop = true; }
    } _StopTask;
public:
    ThreadPool() : _Threads([this](){Worker();}) { _StopTask._Pool = this; }

    ~ThreadPool() {
        EnqueueOne(&_StopTask, 0);
        while (_BlockedThreads)
            WakeOne();
    }

    void EnqueueOne(task* dispatchee, int level) {
        _Dispatcher.EnqueueOne(dispatchee, level);
        WakeOne();
    }
private:
    void Worker() {
        while (!_Stop)
            if (auto job = GetJob(); job)
                job->fire();
    }

    void ThreadEnterIdleRegion() {
        std::unique_lock<std::mutex> __lk(_Mtx);
        ++_BlockedThreads;
        _Cond.wait(__lk);
        --_BlockedThreads;
    }

    task* GetJob() {
        task* job = nullptr;
        size_t iter = 0;
        while (!_Stop && (job = _Dispatcher.DispatchOne()) == nullptr) {
            if (iter++ < MAX_WAIT_ITERS)
                IDLE;
            else {
                ThreadEnterIdleRegion();
                iter = 0;
            }
        }
        return job;
    }

    void WakeOne() {
        if (_BlockedThreads) {
            std::lock_guard<std::mutex> __lk(_Mtx);
            _Cond.notify_one();
        }
    }

    bool _Stop = false;
    std::mutex _Mtx;
    std::condition_variable _Cond;
    std::atomic_int _BlockedThreads = 0;
    BasicDispatcher _Dispatcher;
    ThreadGroup _Threads;
} _SystemPool;

thread_local int _Priority = 7;

void enqueue_one(task* __task, int __priority) { _SystemPool.EnqueueOne(__task, __priority); }

int get_current_thread_priority() noexcept { return _Priority; }
