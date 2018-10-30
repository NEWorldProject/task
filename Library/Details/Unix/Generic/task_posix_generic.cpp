#include "task.h"

#ifdef __TASK_TASK_IMPL_GENERIC
#include <array>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>
#include <iostream>
#include "event.h"
#include "spin_lock.h"
namespace task {
    namespace {
        constexpr int PriorityLevels = 4;
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
        task* _Head = nullptr, *_Tail = nullptr;

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
                for (int i = PriorityLevels - 1; i > -1; --i)
                    if (ret = _DispatchQueues[i].DequeueOne(), ret)
                        break;
                _Lock.leave();
                return ret;
            }
        private:
            spin_lock _Lock;
            std::array<TaskQueue, PriorityLevels> _DispatchQueues;
        };

        class UtilizationProfiler {
            static constexpr auto SegmentBits = 8;
            static constexpr auto Segment = 1 << SegmentBits; // Nanoseconds
            static constexpr auto IdleMask = 0;
            static constexpr auto BusyMask = Segment - 1;
            static long long Count() noexcept {
                return std::chrono::high_resolution_clock::now().time_since_epoch().count();
            }
        public:
            UtilizationProfiler() noexcept {
                _Section = _Zero = static_cast<uint64_t>(Count() >> SegmentBits);
                Sync();
            }
            void Idle() noexcept {
                Sync();
                _Mask = IdleMask;
            }
            void Busy() noexcept {
                Sync();
                _Mask = BusyMask;
            }
            void Sync() noexcept {
                auto _Count = Count();
                auto Current = _Count >> SegmentBits;
                if (_Section - _Zero)
                    _Total = (_Total*(_Section - _Zero) + (_Mask == BusyMask)*(Current - _Section)) / (Current - _Zero);
                _Section = std::max(_Section, uint64_t(Current - 8));
                while (_Section < Current)
                    _Utilize[_Section++ & 7] = _Mask;
                _Utilize[_Section & 7] += static_cast<uint16_t>(_Count & BusyMask);
            }
            auto GetTotal() const noexcept { return _Total; }
            double GetCurrentFast() noexcept {
                Sync();
                return double(_Utilize[_Section & 7]) / BusyMask;
            }
            double GetCurrentInterpolated() noexcept {
                Sync();
                // TODO: Implement Interpolation Here
                return GetCurrentFast();
            }
        private:
            uint16_t _Utilize[7]{};
            uint64_t _Section{}, _Zero{};
            uint16_t _Mask = IdleMask;
            double _Total = 0.0;
        };

        class ThreadGroup {
        public:
            template <class Func>
            explicit ThreadGroup(Func fn) {
                for (auto i = 0; i < std::thread::hardware_concurrency(); ++i)
                    _Threads.emplace_back(fn);
            }

            void JoinAll() noexcept {
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
                :_Threads([this]() { Worker(); }) {
                _StopTask._Pool = this;
            }

            ~ThreadPool() {
                if (_Stop)
                    Stop();
            }

            void Stop() {
                EnqueueOne(&_StopTask, 0);
                _Idle.signal_all();
                while (_BlockedThreads)
                    WakeOne();
                _Threads.JoinAll();
            }

            void EnqueueOne(task* dispatchee, int level) {
                _Dispatcher.EnqueueOne(dispatchee, level);
                WakeOne();
            }
        private:
            std::mutex _T;
            void Worker() {
                UtilizationProfiler prf{};
                while (!_Stop)
                    if (auto job = GetJob(); job) {
                        prf.Busy();
                        job->fire();
                        prf.Idle();
                    }
                {
                    std::lock_guard<std::mutex> l{ _T };
                    std::cout << prf.GetTotal() << std::endl;
                }
            }

            void ThreadEnterIdleRegion() {
                ++_BlockedThreads;
                _Idle.wait();
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

        thread_local int _Priority = 2;
    }

    void __early_stop() noexcept { _SystemPool.Stop(); }

    void enqueue_one(task* __task, int __priority) { _SystemPool.EnqueueOne(__task, __priority); }

    int get_current_thread_priority() noexcept { return _Priority; }
}
#endif
