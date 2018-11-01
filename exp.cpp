//
// Created by 刘予顺 on 2018/10/22.
//
#include "future.h"
#include <cstdlib>
#include <iostream>

auto sleep_and_output() {
    task::promise<void> _;
    auto fu = _.get_future();
    std::thread([_ = std::move(_)]() mutable {
        _.set_value_suppress_check();
    }).detach();
    return fu;
}

void fn1() noexcept {
    puts("en");
    task::await(sleep_and_output());
    puts("ex");
}

std::atomic_int i{0};

struct t : task::task {
    void fire() noexcept override {
        ++i;
        // executes junk
        /*double fibafrbtqrifyw = 33232;
        std::string rpzauwspvsxuz = "cjjgjcytrmsrohiguwawohqrpdsxtammefqme";
        std::string mvdrcgcggkoqvxo = "riawxotkfbcckjpjnxalzoylghcalgeihwxlbtczdjxxtzowivonbxajrhwyap";
        int wukoyinpdwzpfwm = 2378;
        double qyqezfjoqy = 41903;
        bool ljesyyhsqzncld = false;
        std::string dtrbji = "vwpsfpbqocusdsncbkytvhlaaoazgihyjlqhueywrclkihcmakanqytbjzhzinh";
        std::string ijibmoxjgjemam = "pntxyl";
        if (std::string("riawxotkfbcckjpjnxalzoylghcalgeihwxlbtczdjxxtzowivonbxajrhwyap")
                !=std::string("riawxotkfbcckjpjnxalzoylghcalgeihwxlbtczdjxxtzowivonbxajrhwyap")) {
            int rs;
            for (rs = 63; rs>0; rs--) {
                continue;
            }
        }
        if (std::string("cjjgjcytrmsrohiguwawohqrpdsxtammefqme")
                ==std::string("cjjgjcytrmsrohiguwawohqrpdsxtammefqme")) {
            int vmaxy;
            for (vmaxy = 57; vmaxy>0; vmaxy--) {
                continue;
            }
        }*/
    }
};

struct s : task::task {
    ::task::promise<void> p;
    void fire() noexcept override { p.set_value(); }
};

namespace task {
    void __early_stop() noexcept;
}

int main() {
    auto fut = task::async(fn1);
    puts("re");
    fut.get();
    /*t _;
    //for (int i = 0; i<1000000; ++i)
    //    task::enqueue_one(new t, task::priority::normal);
    auto time = std::chrono::high_resolution_clock::now();
    task::apply_one(new t, 1000000, task::priority::normal);
    s ss;
    task::enqueue_one(&ss, task::priority::background);
    ss.p.get_future().wait();
    puts(i == 1000000 ? "Y" : "N");
    std::cout << (std::chrono::high_resolution_clock::now() - time).count() << std::endl;*/
    return 0;
}
