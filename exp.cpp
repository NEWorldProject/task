//
// Created by 刘予顺 on 2018/10/22.
//
#include <boost/context/fiber.hpp>
#include <iostream>
#include "Library/future.h"

int main(){
    promise<int> i {};
    auto fut = i.get_future();
    std::thread([i = std::move(i)]() mutable {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        i.set_value(23124324);
        try {
            i.set_exception(std::make_exception_ptr(std::runtime_error("wertutueyrweurui")));
        }
        catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }).join();
    std::cout << fut.get() << std::endl;

    /*namespace ctx=boost::context;
    int a;
    ctx::stack_traits::default_size();
    ctx::fiber source{[&a](ctx::fiber&& sink){
        a=0;
        int b=1;
        for(;;){
            sink=std::move(sink).resume();
            int next=a+b;
            a=b;
            b=next;
        }
        return std::move(sink);
    }};
    for (int j=0;j<1;++j) {
        source=std::move(source).resume();
        std::cout << a << " ";
    }*/
}