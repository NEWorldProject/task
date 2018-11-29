#pragma once
#include <new>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <condition_variable>

#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define IDLE _mm_pause()
#elif __has_include(<intrin.h>)
#include <intrin.h>
#define IDLE _mm_pause()
#else
#define IDLE
#endif

#include "task.h"

// TODO: Reduce Allocation Calls
namespace task {
    enum class future_errc : int {
        broken_promise = 0,
        future_already_retrieved = 1,
        promise_already_satisfied = 2,
        no_state = 3
    };

    class future_error : public std::logic_error {
    public:
        TASK_API explicit future_error(future_errc ec);

        const auto& code() const noexcept { return __code; }

        [[noreturn]] static void __throw(future_errc ec) { throw future_error(ec); }
    private:
        future_errc __code;
    };

    class __shared_assoc_state_base {
        // Frequent Use, kept apart with others
        mutable std::atomic<uintptr_t> __lock{0};

        // state control
        enum __lck_bits {
            spin_bit = 0b1,
            write_bit = 0b10,
            ready_bit = 0b100,
            pmr_bit_rev = 0b111
        };

        struct alignas(8) __sync_pmr_data {
            std::mutex __m{};
            std::condition_variable __v{};
        };

        auto __get_pmr_address() const noexcept {
            return reinterpret_cast<__sync_pmr_data*>(__lock.load() & (~uintptr_t(pmr_bit_rev)));
        }

        auto __enable_pmr() const {
            auto __new_pmr = new(&__free_store) __sync_pmr_data();
            __lock.fetch_or(reinterpret_cast<uintptr_t>(__new_pmr));
            return __new_pmr;
        }

        void __notify_if_has_pmr() const noexcept {
            if (auto __pmr = __get_pmr_address(); __pmr) {
                std::lock_guard<std::mutex> __lk(__pmr->__m);
                __pmr->__v.notify_all();
            }
        }

        bool __try_acquire_write_access() const noexcept {
            size_t __iters = 0;
            for (;;) {
                while (__check_write_bit() && (!__check_ready_bit()))
                    if (++__iters<100) IDLE; else std::this_thread::yield();
                if (__check_ready_bit()) return false;
                auto _ = __lock.load();
                if (__lock.compare_exchange_strong(_, _ | write_bit, std::memory_order_acquire)) return true;
            }
        }

        bool __check_spin_lock() const noexcept {
            return static_cast<bool>(__lock.load(std::memory_order_relaxed) & spin_bit);
        }

        void __acquire_spin_lock() const noexcept {
            size_t __iters = 0;
            for (;;) {
                while (__check_spin_lock()) if (++__iters<100) IDLE; else std::this_thread::yield();
                auto _ = __lock.load();
                if (__lock.compare_exchange_strong(_, _ | spin_bit, std::memory_order_acquire)) return;
            }
        }

        void __release_spin_lock() const noexcept { __lock.fetch_and(~uintptr_t(spin_bit)); }

        auto __prepare_wait() const {
            if (auto addr = __get_pmr_address(); addr) return addr;
            __acquire_spin_lock();
            auto pmr = __enable_pmr();
            __release_spin_lock();
            return pmr;
        }
    protected:
        void __prepare_write() const {
            auto success = __try_acquire_write_access();
            if (!success) future_error::__throw(future_errc::promise_already_satisfied);
        }

        void __complete_write() noexcept {
            __lock.fetch_or(ready_bit);
            __notify_if_has_pmr();
            __fire_task();
        }

        void __prepare_write_suppress_check() const noexcept { }

        void __complete_write_suppress_check() noexcept {
            __lock.fetch_or(ready_bit | write_bit);
            __notify_if_has_pmr();
            __fire_task();
        }

        void __cancel_write() const noexcept { __lock.fetch_and(~uintptr_t(write_bit)); }
    public:
        void wait() const {
            if (!is_ready()) {
                auto _ = __prepare_wait();
                std::unique_lock<std::mutex> __lk(_->__m);
                while (!is_ready())
                    _->__v.wait(__lk);
            }
        }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) const {
            if (!is_ready()) {
                auto _ = __prepare_wait();
                std::unique_lock<std::mutex> __lk(_->__m);
                while (!is_ready() && _Clock::now()<__abs_time)
                    _->__v.wait_until(__lk, __abs_time);
            }
            return is_ready();
        }

        template <class _Rep, class _Period>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) const {
            return wait_until(std::chrono::steady_clock::now()+__rel_time);
        }

        bool is_ready() const noexcept { return __check_ready_not_expire(); }
    private:
        bool __check_write_bit() const noexcept { return static_cast<bool>(__lock.load() & write_bit); }

        bool __check_ready_bit() const noexcept { return static_cast<bool>(__lock.load() & ready_bit); }

        bool __check_ready_not_expire() const noexcept { return __check_ready_bit() && __check_write_bit(); }

        void __make_expire() const noexcept { __lock.fetch_and(~uintptr_t(write_bit)); }
    protected:
        void __prepare_get() const {
            wait();
            __acquire_spin_lock();
            if (!__check_ready_not_expire()) {
                __release_spin_lock();
                future_error::__throw(future_errc::future_already_retrieved);
            }
            __make_expire();
            __release_spin_lock();
            if (exception_ptr)
                std::rethrow_exception(exception_ptr);
        }
    public:
        bool __is_satisfied() const noexcept { return __check_ready_bit(); }
        bool valid() const noexcept { return __check_ready_not_expire() && _continue==nullptr; }
        // Continuable
    public:
        void __set_task(task* _task) noexcept {
            auto prev = _continue.exchange(_task);
            if (prev==reinterpret_cast<task*>(uintptr_t(~0))) __fire_task();
        }
    private:
        std::atomic<task*> _continue{nullptr};

        task* __get_task() noexcept { return _continue.exchange(reinterpret_cast<task*>(uintptr_t(~0))); }

        void __fire_task() noexcept {
            if (auto __task = __get_task(); __task)
                __task->fire();
        }
    public:
        virtual ~__shared_assoc_state_base() { if (auto _ = __get_pmr_address(); _) _->~__sync_pmr_data(); }

    private:
        // Data Store
        mutable std::atomic_int __ref{0};
        std::exception_ptr exception_ptr = nullptr;
    public:
        void set_exception(std::exception_ptr p) {
            __prepare_write();
            exception_ptr = p;
            __complete_write();
        }

        void set_exception_suppress_check(std::exception_ptr p) {
            __prepare_write_suppress_check();
            exception_ptr = p;
            __complete_write_suppress_check();
        }

        void __acquire() const noexcept { __ref.fetch_add(1); }

        void __release() const noexcept { if (__ref.fetch_sub(1)==1) delete this; }
    private:
        mutable std::aligned_storage_t<sizeof(__sync_pmr_data), alignof(__sync_pmr_data)> __free_store;
    };

    template <class T>
    class __shared_assoc_state final : public __shared_assoc_state_base {
    public:
        ~__shared_assoc_state() { reinterpret_cast<T*>(&__val)->~T(); }

        void set_value(T&& v) {
            __prepare_write();
            if constexpr(std::is_nothrow_move_constructible_v<T>)
                new(&__val) T(std::forward<T>(v));
            else
                try {
                    new(&__val) T(std::forward<T>(v));
                }
                catch (...) {
                    __cancel_write();
                    throw;
                }
            __complete_write();
        }

        void set_value(const T& v) {
            __prepare_write();
            if constexpr(std::is_nothrow_copy_constructible_v<T>)
                new(&__val) T(v);
            else
                try {
                    new(&__val) T(v);
                }
                catch (...) {
                    __cancel_write();
                    throw;
                }
            __complete_write();
        }

        void set_value_suppress_check(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) {
            __prepare_write_suppress_check();
            new(&__val) T(std::forward<T>(v));
            __complete_write_suppress_check();
        }

        void set_value_suppress_check(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
            __prepare_write_suppress_check();
            new(&__val) T(v);
            __complete_write_suppress_check();
        }

        T get() {
            __prepare_get();
            return std::move(*reinterpret_cast<T*>(&__val));
        }
    private:
        std::aligned_storage_t<sizeof(T), alignof(T)> __val;
    };

    template <class T>
    class __shared_assoc_state<T&> final : public __shared_assoc_state_base {
    public:
        void set_value(T& v) {
            __prepare_write();
            __val = std::addressof(v);
            __complete_write();
        }

        void set_value_suppress_check(T& v) noexcept {
            __prepare_write_suppress_check();
            __val = std::addressof(v);
            __complete_write_suppress_check();
        }

        T& get() {
            __prepare_get();
            return *__val;
        }
    private:
        T* __val;
    };

    template <>
    class __shared_assoc_state<void> final : public __shared_assoc_state_base {
    public:
        void set_value() {
            __prepare_write();
            __complete_write();
        }

        void set_value_suppress_check() noexcept {
            __prepare_write_suppress_check();
            __complete_write_suppress_check();
        }

        void get() const { __prepare_get(); }
    };

    template <class T>
    class __promise_base;

    template <class T>
    class future;

    template <class T>
    class promise;

    template <class Callable, class ...Ts>
    class defer_callable {
        template <std::size_t... I>
        auto apply_impl(std::index_sequence<I...>) { return __fn(std::move(std::get<I>(__args))...); }
    protected:
        using return_type = std::invoke_result_t<std::decay_t<Callable>, std::decay_t<Ts>...>;
    public:
        explicit defer_callable(Callable&& call, Ts&& ... args)
                :__fn(std::forward<Callable>(call)), __args(std::forward<Ts>(args)...) { }
        auto call() { return apply_impl(std::make_index_sequence<std::tuple_size<decltype(__args)>::value>()); }
    private:
        Callable __fn;
        std::tuple<Ts...> __args;
    };

    template <class Callable>
    class defer_callable<Callable> {
    protected:
        using return_type = std::invoke_result_t<std::decay_t<Callable>>;
    public:
        explicit defer_callable(Callable&& call)
                :__fn(std::forward<Callable>(call)) { }
        auto call() { return __fn(); }
    private:
        Callable __fn;
    };

    template <class Callable, class ...Ts>
    class defer_procedure_call_task : public task, defer_callable<Callable, Ts...> {
    public:
        using return_type = typename defer_callable<Callable, Ts...>::return_type;

        explicit defer_procedure_call_task(Callable&& call, Ts&& ... args)
                :defer_callable<Callable, Ts...>(std::forward<Callable>(call), std::forward<Ts>(args)...) { }

        void fire() noexcept override {
            try {
                if constexpr(std::is_same_v<return_type, void>) {
                    defer_callable<Callable, Ts...>::call();
                    __promise.set_value_suppress_check();
                }
                else
                    __promise.set_value_suppress_check(defer_callable<Callable, Ts...>::call());
            }
            catch (...) {
                __promise.set_exception_suppress_check(std::current_exception());
            }
            delete this;
        }

        auto get_future() { return __promise.get_future(); }
    private:
        promise<return_type> __promise{};
    };

    template <class T>
    class __future_base {
    protected:
        explicit __future_base(__shared_assoc_state<T>* _) noexcept
                :__st(_) { __st->__acquire(); }
    public:
        __future_base() = default;

        __future_base(__future_base&& other) noexcept
                :__st(other.__st) { other.__st = nullptr; }

        __future_base& operator=(__future_base&& other) noexcept {
            if (this!=std::addressof(other)) {
                __st = other.__st;
                other.__st = nullptr;
            }
            return *this;
        }
        __future_base(const __future_base&) = delete;

        __future_base& operator=(const __future_base&) = delete;

        ~__future_base() { __release_state(); }

        bool valid() const noexcept { return __st ? __st->valid() : false; }

        bool is_ready() const noexcept { return __st->is_ready(); }

        void wait() const { __st->wait(); }

        template <class _Clock, class _Duration>
        bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __time) const {
            return __st->wait_until(__time);
        }

        template <class _Rep, class _Period>
        bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) const {
            return __st->wait_until(__rel_time);
        }

        template <class Func>
        auto then(Func fn) {
            auto __task = std::make_unique<defer_procedure_call_task<std::decay_t<Func>, future<T>>>(
                    std::forward<std::decay_t<Func>>(std::move(fn)),
                    future(nullptr, __st)
            );
            auto __fut = __task->get_future();
            __st->__set_task(__task.release());
            __st = nullptr;
            return __fut;
        }
    protected:
        void __release_state() noexcept {
            if (__st) {
                __st->__release();
                __st = nullptr;
            }
        }

        struct __release_guard {
            explicit __release_guard(__future_base* _) noexcept
                    :__this(_) { }
            __release_guard(__release_guard&&) = delete;
            __release_guard& operator=(__release_guard&&) = delete;
            ~__release_guard() noexcept { __this->__release_state(); }
            __future_base* __this;
        };

        auto __create_guard() noexcept { return __release_guard(this); }

        __shared_assoc_state<T>* __st = nullptr;
    };

    template <class T>
    class future : public __future_base<T> {
        friend class __promise_base<T>;
        friend class __future_base<T>;
        explicit future(__shared_assoc_state<T>* _) noexcept
                :__future_base<T>(_) { }

        future(std::nullptr_t, __shared_assoc_state<T>* _) noexcept { __future_base<T>::template __st = _; }
    public:
        future() noexcept = default;
        auto get() {
            auto _ = __future_base<T>::template __create_guard();
            return __future_base<T>::template __st->get();
        }
    };

    template <class T>
    class future<T&> : public __future_base<T&> {
        friend class __promise_base<T&>;
        friend class __future_base<T&>;
        explicit future(__shared_assoc_state<T&>* _) noexcept
                :__future_base<T&>(_) { }

        future(std::nullptr_t, __shared_assoc_state<T&>* _) noexcept { __future_base<T&>::template __st = _; }
    public:
        future() noexcept = default;
        auto& get() {
            auto _ = __future_base<T&>::template __create_guard();
            return __future_base<T&>::template __st->get();
        }
    };

    template <>
    class future<void> : public __future_base<void> {
        friend class __promise_base<void>;
        friend class __future_base<void>;
        explicit future(__shared_assoc_state<void>* _) noexcept
                :__future_base<void>(_) { }

        future(std::nullptr_t, __shared_assoc_state<void>* _) noexcept { __st = _; }
    public:
        future() noexcept = default;
        void get() {
            auto _ = __create_guard();
            __st->get();
        }
    };

    template <class T>
    class __promise_base {
    public:
        __promise_base() = default;

        __promise_base(__promise_base&& other) noexcept
                :__st(other.__st) { other.__st = nullptr; }

        __promise_base& operator=(__promise_base&& other) noexcept {
            if (this!=std::addressof(other)) {
                __st = other.__st;
                other.__st = nullptr;
            }
            return *this;
        }

        __promise_base(const __promise_base&) = delete;

        __promise_base& operator=(const __promise_base&) = delete;

        ~__promise_base() noexcept {
            if (__st) {
                if (!__st->__is_satisfied())
                    set_exception_suppress_check(std::make_exception_ptr(future_error(future_errc::broken_promise)));
                __st->__release();
            }
        }

        future<T> get_future() { return future<T>(__st); }

        void set_exception(std::exception_ptr p) { __st->set_exception(p); };

        void set_exception_suppress_check(std::exception_ptr p) noexcept { __st->set_exception_suppress_check(p); }
    protected:
        __shared_assoc_state<T>& __get_state() {
            if (__st) return *__st;
            future_error::__throw(future_errc::no_state);
        }
        __shared_assoc_state<T>* __st = make_assoc_state();
    private:
        __shared_assoc_state<T>* make_assoc_state() {
            auto _ = new __shared_assoc_state<T>;
            _->__acquire();
            return _;
        }
    };

    template <class T>
    class promise : public __promise_base<T> {
    public:
        void set_value(T&& v) { __promise_base<T>::template __get_state().set_value(std::move(v)); }

        void set_value(const T& v) { __promise_base<T>::template __get_state().set_value(v); }

        void set_value_suppress_check(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) {
            __promise_base<T>::template __get_state().set_value_suppress_check(std::move(v));
        }

        void set_value_suppress_check(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
            __promise_base<T>::template __get_state().set_value_suppress_check(v);
        }
    };

    template <class T>
    class promise<T&> : public __promise_base<T&> {
    public:
        void set_value(T& v) { __promise_base<T&>::template __get_state().set_value(v); }

        void set_value_suppress_check(T& v) noexcept {
            __promise_base<T&>::template __get_state().set_value_suppress_check(v);
        }
    };

    template <>
    class promise<void> : public __promise_base<void> {
    public:
        void set_value() { __get_state().set_value(); }

        void set_value_suppress_check() noexcept { __get_state().set_value_suppress_check(); }
    };

    namespace __detail {
        template <class T>
        struct __packed_task_extract_base {
            using __base = T;
        };
    }

    template <class T>
    class packaged_task;

    template <class R, class ...Args>
    class packaged_task<R(Args...)> : __detail::__packed_task_extract_base<R(Args...)> {

    };

#if __has_include(<concrt.h>)
}
#include <concrt.h>
namespace task {
    template <class Func, class ...Ts>
    auto async(Func __fn, Ts&& ... args) {
        auto inner_task = new defer_procedure_call_task(
                std::forward<std::decay_t<Func>>(std::move(__fn)),
                std::forward<Ts>(args)...
        );
        auto future = inner_task->get_future();
        enqueue_one(inner_task, get_current_thread_priority());
        return future;
    }

    template <template <class> class Cont, class U>
    U await(Cont<U> cont) {
        if constexpr (std::is_same_v<U, void>) {
            auto fu = cont.then([ctx = Concurrency::Context::CurrentContext()](auto&& lst) {
                ctx->Unblock();
                lst.get();
            });
            Concurrency::Context::Block();
            fu.get();
        }
        else {
            auto fu = cont.then([ctx = Concurrency::Context::CurrentContext()](auto&& lst) {
                ctx->Unblock();
                return lst.get();
            });
            Concurrency::Context::Block();
            return fu.get();
        }
        if constexpr (std::is_same_v<U, void>)
            cont.get()
        else
            return cont.get()
    }

#else
    void __async_call(task*) noexcept;

    void __async_resume_previous() noexcept;

    task* __async_get_current() noexcept;

    template <template <class> class Cont, class U>
    U await(Cont<U> cont) {
        if constexpr (std::is_same_v<U, void>) {
            auto fu = cont.then([task = __async_get_current()](auto&& lst) {
                enqueue_one(task, get_current_thread_priority());
                lst.get();
            });
            __async_resume_previous();
            fu.get();
        }
        else {
            auto fu = cont.then([task = __async_get_current()](auto&& lst) {
                enqueue_one(task, get_current_thread_priority());
                return lst.get();
            });
            __async_resume_previous();
            return fu.get();
        }
    }

    template <class Func, class ...Ts>
    auto async(Func __fn, Ts&& ... args) {
        auto inner_task = new defer_procedure_call_task(
                std::forward<std::decay_t<Func>>(std::move(__fn)),
                std::forward<Ts>(args)...
        );
        auto future = inner_task->get_future();
        __async_call(inner_task);
        return future;
    }
#endif
}
