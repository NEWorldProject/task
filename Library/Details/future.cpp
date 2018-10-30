#include "future.h"

namespace task {
    namespace {
        const char* __get_error_string(future_errc __code) noexcept {
            switch (__code) {
            case future_errc::broken_promise: return "future_error:broken promise";
            case future_errc::future_already_retrieved: return "future_error:future_already_retrieved";
            case future_errc::promise_already_satisfied: return "future_error:promise_already_satisfied";
            case future_errc::no_state: return "future_error:no_state";
            }
        }
    }

    future_error::future_error(future_errc ec):logic_error(__get_error_string(ec)), __code(ec) { }
}