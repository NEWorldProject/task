
/*concept Continuable {
    Continuable then(Callable, Args...);
};*/

#include <memory>
#include <atomic>
#include <emmintrin.h>
#include <thread>
#include <deque>

#define MAX_WAIT_ITERS 50

struct Dispatchee {
    virtual void Contiune() =
};

constexpr int PriorityLevels = 128;
constexpr int hardware_destructive_interference_size = 64;

class SpinLock {
public:
    void Enter() { while (Locked.exchange(true, std::memory_order_acquire)) WaitUntilLockIsFree(); }

    void Leave() { Locked.store(false, std::memory_order_relaxed); }

private:
    static void CpuRelax() {
        _mm_pause();
/*#elif (COMPILER == GCC || COMPILER == LLVM)
        asm("pause");
#endif*/
    }

    void WaitUntilLockIsFree() const {
        size_t numIters = 0;

        while (Locked.load(std::memory_order_relaxed)) {
            if (numIters < MAX_WAIT_ITERS) {
                CpuRelax();
                ++numIters;
            }
            else
                std::this_thread::yield();
        }
    }
    alignas(hardware_destructive_interference_size) std::atomic_bool Locked = {false};
};

class ASyncTaskQueue {
public:
    void EnqueueOne(std::unique_ptr<Dispatchee> dispatchee) {
        _Lock.Enter();
        _Queue.push_back(std::move(dispatchee));
        _Lock.Leave();
    }

    std::unique_ptr<Dispatchee> DequeueOne() {
        std::unique_ptr<Dispatchee> ret = nullptr;
        _Lock.Enter();
        if (!Empty()) {
            ret = std::move(_Queue.front());
            _Queue.pop_front();
        }
        _Lock.Leave();
    }

    bool Empty() const noexcept { return _Queue.empty(); }
private:
    std::deque<std::unique_ptr<Dispatchee>> _Queue;
    SpinLock _Lock;
};

class BasicDispatcher {
public:
    void EnqueueOne(std::unique_ptr<Dispatchee> dispatchee, int level) {
        _DispatchQueues[level].EnqueueOne(std::move(dispatchee));
    }

    std::unique_ptr<Dispatchee> DispatchOne() {
        std::unique_ptr<Dispatchee> ret = nullptr;
        for(int i = PriorityLevels; i > -1; --i)
            if (ret = std::move(_DispatchQueues->DequeueOne()), ret)
                return ret;
        return ret;
    }
private:
    ASyncTaskQueue _DispatchQueues[PriorityLevels];
};

class FiberDispatcher : public BasicDispatcher {

};

class TaskDispatcher : public BasicDispatcher {

};

class ExtNotificationDispatcher : public BasicDispatcher {

};

class Dispatcher {

};