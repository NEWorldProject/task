#pragma once

struct task {
    virtual ~task() = default;
    virtual void fire() = 0;
};
