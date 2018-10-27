//
// Created by 刘予顺 on 2018/10/22.
//
#include "Library/future.h"
#include <cstdlib>

using namespace task;

auto sleep_and_output() {
    promise<void> _;
    auto fu = _.get_future();
    std::thread([_ = std::move(_)]() mutable {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        puts("e1");
        _.set_exception_suppress_check(std::make_exception_ptr(std::runtime_error("some error")));
    }).detach();
    return fu;
}

void fn1() noexcept {
    puts("en");
    await(sleep_and_output());
    puts("ex");
}

int main() {
    auto fu = async(fn1);
    puts("re");
    fu.get();
    puts("do");
}
