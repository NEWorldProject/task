#pragma once

#include <cstddef>
#include <mutex>
#include <condition_variable>
#if __has_include("x86intrin.h")
#include <x86intrin.h>
#elif __has_include("intrin.h")
#include <intrin.h>
#endif
#include <thread>

constexpr int hardware_destructive_interference_size = 64;

class __shared_assoc_state_base {
    enum _val_shared_assoc_ {
        sync = 0, aval = 0b001, lck = 0b011, done = 0b111
    };
    struct alignas(8) __sync_pmr_data {
        __sync_pmr_data(){puts("__sync_create");}
        ~__sync_pmr_data(){puts("__sync_destroy");}
        std::mutex __m{};
        std::condition_variable __v{};
    };
    static auto __head(uintptr_t _) noexcept { return _ & (~uintptr_t(0)-0b111); }
    static auto __sync_cast(uintptr_t _) noexcept { return reinterpret_cast<__sync_pmr_data*>(__head(_)); }
public:
    virtual ~__shared_assoc_state_base() {
        auto _ = __sync_cast(__lock);
        delete _;
    }

    bool is_ready() const noexcept { return ((long long) (__lock.load()) & 0b110)==0b110; }

    void wait() {
        if (!is_ready()) {
            auto _ = __wait_init();
            std::unique_lock<std::mutex> __lk(_->__m);
            while (!is_ready())
                _->__v.wait(__lk);
        }
    }

    template <class _Clock, class _Duration>
    bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __abs_time) const {
        if (!is_ready()) {
            auto _ = __wait_init();
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

    void set_exception(std::exception_ptr p) {
        __excp = p;
        __make_ready();
    }

    void __increase_ref() noexcept { __ref.fetch_add(1); }

    void __release_ref() noexcept {
        if (__ref.fetch_sub(1) == 1)
            delete this;
    }
protected:
    void __make_ready() {
        auto _ = __acquire_lock();
        if (_== aval) {
            __lock = done;
        }
        else {
            auto __ = __sync_cast(_);
            std::unique_lock<std::mutex> __lk(__->__m);
            __lock |= 0b110;
            __->__v.notify_all();
        }
    }

    void __get() {
        wait();
        if (__excp)
            std::rethrow_exception(__excp);
    }
private:
    uintptr_t __acquire_lock() const noexcept {
        for (;;) {
            uintptr_t _ =  aval;
            bool succ = __lock.compare_exchange_strong(_,  lck, std::memory_order_acquire);
            if (succ || (_!= lck)) return _;
            __wait_until_lock_is_free();
        }
    }
    __sync_pmr_data* __wait_init() const {
        auto _ = __acquire_lock();
        if (_ == aval)
            __enable_sync_pmr_in_lck();
        return __sync_cast(__lock);
    }

    void __enable_sync_pmr_in_lck() const { __lock = reinterpret_cast<uintptr_t>(new __sync_pmr_data()); }

    void __wait_until_lock_is_free() const noexcept {
        size_t numIters = 0;
        while (__lock.load(std::memory_order_relaxed)) {
            if (numIters<100) {
                _mm_pause();
                ++numIters;
            }
            else
                std::this_thread::yield();
        }
    }
    std::exception_ptr __excp = nullptr;
    alignas(hardware_destructive_interference_size) mutable std::atomic_int __ref{0};
    alignas(hardware_destructive_interference_size) mutable std::atomic<uintptr_t> __lock{ aval};
};

template <class T>
class __shared_assoc_state : public __shared_assoc_state_base {
public:
    void set_value(T&& v) {
        __val = std::move(v);
        __make_ready();
    }

    void set_value(const T& v) {
        __val = v;
        __make_ready();
    }

    T get() { __get(); return __val; }
private:
    T __val;
};

template <class T>
class __shared_assoc_state<T&> : public __shared_assoc_state_base {
public:
    void set_value(T& v) {
        __val = std::addressof(v);
        __make_ready();
    }

    T& get() { __get(); return *__val; }
private:
    T* __val;
};

template <>
class __shared_assoc_state<void> : public __shared_assoc_state_base {
public:
    void set_value() { __make_ready(); }

    void get() { __get(); }
};

template <class T> class promise;

template <class T>
class future {
    friend class promise<T>;
    explicit future(__shared_assoc_state<T>* _) noexcept : __st(_) { __st->__increase_ref(); }
public:
    ~future() { if (__st) __st->__release_ref(); }

    bool valid() const noexcept { return __st; }

    auto get() { return __st->get(); }

    void wait() { __st->wait(); }

    template <class _Clock, class _Duration>
    bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __time) const { return __st->wait_until(__time); }

    template <class _Rep, class _Period>
    bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) const { return __st->wait_until(__rel_time); }
private:
    __shared_assoc_state<T>* __st = nullptr;
};

template <class T>
class future<T&> {
    friend class promise<T&>;
    explicit future(__shared_assoc_state<T&>* _) noexcept : __st(_) { __st->__increase_ref(); }
public:
    ~future() { if (__st) __st->__release_ref(); }

    bool valid() const noexcept { return __st; }

    auto& get() { return __st->get(); }

    void wait() { __st->wait(); }

    template <class _Clock, class _Duration>
    bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __time) const { return __st->wait_until(__time); }

    template <class _Rep, class _Period>
    bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) const { return __st->wait_until(__rel_time); }
private:
    __shared_assoc_state<T&>* __st = nullptr;
};

template <>
class future<void> {
    friend class promise<void>;
    explicit future(__shared_assoc_state<void>* _) noexcept : __st(_) { __st->__increase_ref(); }
public:
    ~future() { if (__st) __st->__release_ref(); }

    bool valid() const noexcept { return __st; }

    void wait() { __st->wait(); }

    void get() { __st->get(); }

    template <class _Clock, class _Duration>
    bool wait_until(const std::chrono::time_point<_Clock, _Duration>& __time) const { return __st->wait_until(__time); }

    template <class _Rep, class _Period>
    bool wait_for(const std::chrono::duration<_Rep, _Period>& __rel_time) const { return __st->wait_until(__rel_time); }
private:
    __shared_assoc_state<void>* __st = nullptr;
};

template <class T>
class promise {
public:
    ~promise() { if (__st) __st->__release_ref(); }

    future<T> get_future() { return future<T>(__st); }

    void set_value(T&& v) { __st->set_value(std::move(v)); }

    void set_value(const T& v) { __st->set_value(v); }

    void set_exception(std::exception_ptr p) { __st->set_exception(p); };
private:
    __shared_assoc_state<T>* __st = make_assoc_state();
    __shared_assoc_state<T>* make_assoc_state() {
        auto _ = new __shared_assoc_state<T>;
        _->__increase_ref();
        return _;
    }
};

template <class T>
class promise<T&> {
public:
    ~promise() { if (__st) __st->__release_ref(); }

    future<T&> get_future() { return future<T&>(__st); }

    void set_value(T& v) { __st->set_value(v); }

    void set_exception(std::exception_ptr p) { __st->set_exception(p); };
private:
    __shared_assoc_state<T&>* __st = make_assoc_state();
    __shared_assoc_state<T&>* make_assoc_state() {
        auto _ = new __shared_assoc_state<T&>;
        _->__increase_ref();
        return _;
    }
};

template <>
class promise<void> {
public:
    ~promise() { if (__st) __st->__release_ref(); }

    future<void> get_future() { return future<void>(__st); }

    void set_value() { __st->set_value(); }

    void set_exception(std::exception_ptr p) { __st->set_exception(p); };
private:
    __shared_assoc_state<void>* __st = make_assoc_state();
    __shared_assoc_state<void>* make_assoc_state() {
        auto _ = new __shared_assoc_state<void>;
        _->__increase_ref();
        return _;
    }
};
