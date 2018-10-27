//
// Created by 刘予顺 on 2018/10/25.
//

#include <boost/context/fiber.hpp>
#include "future.h"

namespace {
    class async_exec_task : public task::task {
    public:
        explicit async_exec_task(std::function<void()> fn)
                :
                _Current(boost::context::fiber([this, fn = std::move(fn)](boost::context::fiber&& sink) mutable {
                    _Sink = std::move(sink);
                    fn();
                    return std::move(_Sink);
                })) { }
        void fire() noexcept override;
        void ResumeSink() { _Sink = std::move(_Sink).resume(); }
    private:
        boost::context::fiber _Current, _Sink;
    };

    thread_local async_exec_task* exec_task = nullptr;

    void async_exec_task::fire() noexcept {
        exec_task = this;
        _Current = std::move(_Current).resume();
        if (!static_cast<bool>(_Current))
            delete this;
    }
}

namespace task {
    void __async_resume_previous() noexcept { exec_task->ResumeSink(); }

    task* __async_get_current() noexcept { return exec_task; }

    void __async_call(std::function<void()> fn) noexcept { (new async_exec_task(std::move(fn)))->fire(); }
}
