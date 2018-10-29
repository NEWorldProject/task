//
// Created by 刘予顺 on 2018/10/22.
//
#include "Library/future.h"
#include <cstdlib>
#include <iostream>

/*auto sleep_and_output() {
    promise<void> _;
    auto fu = _.get_future();
    std::thread([_ = std::move(_)]() mutable {
        _.set_value_suppress_check();
    }).detach();
    return fu;
}

void fn1() noexcept {
    puts("en");
    await(sleep_and_output());
    puts("ex");
}*/

std::atomic_int i {0};
struct t : task::task {
    void fire() noexcept override { ++i; }
};

struct s : task::task {
    ::task::promise<void> p;
    void fire() noexcept override { p.set_value(); }
};

int main() {
    t _;
    for (int i = 0; i < 1000000; ++i)
        task::enqueue_one(new t, 7);
    s ss;
    task::enqueue_one(&ss, 6);
    puts(i == 1000000 ? "Y" : "N");
    ss.p.get_future().wait();
    return 0;
}
