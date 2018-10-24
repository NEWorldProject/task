//
// Created by 刘予顺 on 2018/10/22.
//
#include <boost/context/fiber.hpp>
#include <iostream>
#include "Library/future.h"

/*template <class Func, class ...Ts>
void async_invoke(Func __fn, Ts&&... args) {
    constexpr bool __noexcept_inv = noexcept(__fn(std::forward<Ts>(args)...));
    using __result_t = std::result_of_t<std::decay_t<Func>(Ts...)>;
    promise<__result_t> __promise;
    try {

    }

}*/

int main(){
    promise<void> i {};
    auto fut = i.get_future().then([](auto&& fut) {
        std::cout << "done"<< std::endl;
    });
    std::thread([i = std::move(i)]() mutable {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        i.set_value();
        try {
            i.set_exception(std::make_exception_ptr(std::runtime_error("wertutueyrweurui")));
        }
        catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }).join();

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