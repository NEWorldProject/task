#pragma once

struct task {
    virtual ~task() = default;
    virtual void fire() noexcept = 0;
};

void enqueue_one(task* __task, int __priority);

int get_current_thread_priority() noexcept;
