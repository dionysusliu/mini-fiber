#include <cassert>
#include "minifiber/baton.hpp"

namespace minifiber {

void Baton::wait() {
    if (st_ == St::Posted) {
        st_ = St::Init; // early post, consume it
        return;
    }

    assert(waiter_ == nullptr && "Baton: concurrent waiters"); // at most one waiter on one baton
    st_ = St::Waiting;
    waiter_ = detail::current();
    detail::suspend_current();
    // --- blocked fiber resumed from there --- 
}

void Baton::post() {
    if (st_ == St::Waiting) {
        Fiber* w = waiter_; 
        st_ = St::Init;
        waiter_ = nullptr;
        detail::wake(w); // wake up the waiter
    } else {
        st_ = St::Posted;
    }
}

}