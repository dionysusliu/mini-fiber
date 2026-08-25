#include <cassert>
#include "minifiber/fiber.hpp"
#include "minifiber/scheduler.hpp"
#include "minifiber/round_robin.hpp"

namespace minifiber {
namespace {

constexpr std::size_t kStackSize = 64 * 1024;

// global states of runtime
Context g_sched_ctx;
Fiber *g_current;
std::unique_ptr<Scheduler> g_sched = std::make_unique<RoundRobinScheduler>();


// trampoline, for context switch
static void trampoline() {
    // fiber entry
    g_current->fn(); 
    // fiber yields / exit
    g_current->state = State::Finished;
    // on return we should change uc_link to 'scheduler'
    switch_context(&g_current->ctx, &g_sched_ctx);
    __builtin_unreachable(); // finished fiber would not be woken again 
}

/// @brief  [rsp] = trampoline
/// @param f fiber to prepare
void prepare_context(Fiber *f) {
    uintptr_t sp = reinterpret_cast<uintptr_t>(f->stack.data() + f->stack.size());
    // rsp 是栈顶，rsp-8是第一个可写区域，等价于：
    //  rsp -= 8
    // [rsp] = trampoline
    sp = (sp - 8) & ~uintptr_t(15); // SystemV ABI: 函数入口 rsp ≡ 8 (mod 16)
    *reinterpret_cast<uintptr_t *>(sp) = reinterpret_cast<uintptr_t>(&trampoline);
    f->ctx.rsp = sp;
}

} // namespace 

void use_scheduler(std::unique_ptr<Scheduler> sched) {
    g_sched = std::move(sched);
}

void spawn(std::function<void()> fn, int priority) {
    Fiber *f = new Fiber();
    f->fn = std::move(fn);
    f->priority = priority;
    f->stack.resize(kStackSize);
    prepare_context(f);
    g_sched->awakened(f); // register the fiber
}

void yield() { // called by fiber
    g_current->state = State::Ready;
    g_sched->awakened(g_current);
    switch_context(&g_current->ctx, &g_sched_ctx);
}

void run() {
    while (g_sched->has_ready_fibers()) {
        Fiber *f = g_sched->pick_next();
        f->state = State::Running;
        g_current = f;

        switch_context(&g_sched_ctx, &f->ctx);

        g_current = nullptr;
        if (f->state == State::Finished) delete f;
    }
}

namespace detail {

Fiber* current() { return g_current; }

void suspend_current() {
    g_current->state = State::Blocked;
    switch_context(&g_current->ctx, &g_sched_ctx);
}

void wake(Fiber *f) {
    assert(f->state == State::Blocked); // no double wake / mis-wake
    f->state = State::Ready;
    g_sched->awakened(f);
}

}

}
