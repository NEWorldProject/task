//
// Created by 刘予顺 on 2018/10/22.
//
#include "future.h"
#include "event.h"
#include <cstdlib>
#include <iostream>

auto sleep_and_output() {
    return task::async([]() noexcept{
        std::this_thread::sleep_for(std::chrono::seconds(2));
    });
}

void f() noexcept {
    task::await(sleep_and_output());
}

void h() noexcept { task::await(task::async(f)); }

void j() noexcept { task::await(task::async(h)); }

void fn1() noexcept {
    puts("en");
    task::await(task::async(j));
    puts("ex");
}

std::atomic_int i{1000000};
task::event nti;

void taskf() {
    // executes junk
    double fibafrbtqrifyw = 33232;
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
    }
    if (i.fetch_sub(1) == 1) nti.set();
}

struct t : task::task {
    void fire() noexcept override {
        taskf();
    }
};

struct s : task::task {
    ::task::promise<void> p;
    void fire() noexcept override { p.set_value(); }
};

namespace task {
    void __early_stop() noexcept;
}

std::atomic_int g = 100;

int main() {
    /*auto fut = task::async(fn1);
    puts("re");
    task::event eve;
    for (int i = 0; i < 100; ++i) {
    task::enqueue_one(new task::defer_procedure_call_task([&](){
        std::this_thread::sleep_for(std::chrono::seconds(10));
        puts("done");
        if (g.fetch_sub(1) == 1)
            eve.set();
    }), task::priority::normal);
    }*/
    //for (int i = 0; i<1000000; ++i)
    //    task::enqueue_one(new t, task::priority::normal);
    auto time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i<1000000; ++i)
        task::async(taskf);
    //task::apply_one(new t, 1000000, task::priority::normal);
    nti.wait();
    puts(i == 1000000 ? "Y" : "N");
    std::cout << (std::chrono::high_resolution_clock::now() - time).count() << std::endl;
    return 0;
}
