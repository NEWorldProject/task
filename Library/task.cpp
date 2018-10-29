#include <array>
#include <atomic>
#include <thread>
#include <vector>
#include "task.h"
#include "event.h"
#include "spin_lock.h"

namespace task {
    namespace {
        constexpr int PriorityLevels = 16;
    }

    class TaskQueue {
    public:
        void EnqueueOne(task* dispatchee) noexcept {
            if (!_Head)
                _Head = _Tail = dispatchee;
            else {
                _Tail->__next = dispatchee;
                _Tail = dispatchee;
            }
        }

        task* DequeueOne() noexcept {
            auto ret = _Head;
            if (ret) {
                _Head = _Head->__next;
                Orphan(ret);
            }
            return ret;
        }

        bool Empty() const noexcept { return _Head->__next; }
    private:
        task* _Head = nullptr, * _Tail = nullptr;

        static void Orphan(task* _) noexcept { _->__next = nullptr; }
    };

    namespace {
        class BasicDispatcher {
        public:
            void EnqueueOne(task* dispatchee, int level) noexcept {
                _Lock.enter();
                _DispatchQueues[level].EnqueueOne(dispatchee);
                _Lock.leave();
            }

            task* DispatchOne() noexcept {
                task* ret = nullptr;
                _Lock.enter();
                for (int i = PriorityLevels-1; i>-1; --i)
                    if (ret = _DispatchQueues[i].DequeueOne(), ret)
                        break;
                _Lock.leave();
                return ret;
            }
        private:
            spin_lock _Lock;
            std::array<TaskQueue, PriorityLevels> _DispatchQueues;
        };

        class ThreadGroup {
        public:
            template <class Func>
            explicit ThreadGroup(Func fn) {
                for (auto i = 0; i<std::thread::hardware_concurrency(); ++i)
                    _Threads.emplace_back(fn);
            }

            ~ThreadGroup() noexcept {
                for (auto&& x : _Threads)
                    if (x.joinable())
                        x.join();
            }
        private:
            std::vector<std::thread> _Threads;
        };

        class ThreadPool {
        public:
            ThreadPool()
                    :_Threads([this]() { Worker(); }) { _StopTask._Pool = this; }

            ~ThreadPool() {
                EnqueueOne(&_StopTask, 0);
                _Idle.signal_all();
                while(_BlockedThreads)
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
                ++_BlockedThreads;
                _Idle.wait();
                --_BlockedThreads;
            }

            task* GetJob() {
                task* job = nullptr;
                size_t iter = 0;
                while (!_Stop && (job = _Dispatcher.DispatchOne())==nullptr) {
                    if (iter++<MAX_WAIT_ITERS)
                        IDLE;
                    else {
                        ThreadEnterIdleRegion();
                        iter = 0;
                    }
                }
                return job;
            }

            void WakeOne() {
                if (_BlockedThreads)
                    _Idle.signal_one();
            }

            struct : task {
                ThreadPool* _Pool = nullptr;
                void fire() noexcept override { _Pool->_Stop = true; }
            } _StopTask;

            bool _Stop = false;
            notification _Idle;
            std::atomic_int _BlockedThreads = 0;
            BasicDispatcher _Dispatcher;
            ThreadGroup _Threads;
        } _SystemPool;

        thread_local int _Priority = 7;
    }

    void enqueue_one(task* __task, int __priority) { _SystemPool.EnqueueOne(__task, __priority); }

    int get_current_thread_priority() noexcept { return _Priority; }
}
