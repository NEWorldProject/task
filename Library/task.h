#pragma once

namespace task {
    class task {
        friend class TaskQueue;
    public:
        virtual ~task() = default;
        virtual void fire() noexcept = 0;
        virtual bool reuse() const noexcept { return false; }
    private:
        task* __next = nullptr;
    };

    void enqueue_one(task* __task, int __priority);

    int get_current_thread_priority() noexcept;
}
