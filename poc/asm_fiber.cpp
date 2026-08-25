// poc/ucontext_fiber.cpp - fiber prototype implemented via ucontext

#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <vector>

static const size_t DEFAULT_STACK_SZ = 64 * 1024;

struct Context {
    uint64_t rbx, rbp, r12, r13, r14, r15, rsp;
};

extern "C" void switch_context(Context *from, Context *to);

struct Fiber {
    Context ctx; // register snapshot
    std::vector<char> stack; // fiber stack
    std::function<void()> fn;
    bool finished = false; // fiber state
};

// scheduler
static Context g_sched_ctx; // global scheduler context
static Fiber* g_current = nullptr; // running fiber
static std::deque<Fiber*> g_ready; // FIFO ready queue

// trampoline, for context switch
static void trampoline() {
    // fiber entry
    g_current->fn(); 
    // fiber yields / exit
    g_current->finished = true;
    // on return we should change uc_link to 'scheduler'
    switch_context(&g_current->ctx, &g_sched_ctx);
    __builtin_unreachable(); // finished fiber would not be woken again 
}

/// @brief  [rsp] = trampoline
/// @param f fiber to prepare
void _prepare_context(Fiber *f) {
    uintptr_t sp = reinterpret_cast<uintptr_t>(f->stack.data() + f->stack.size());
    // rsp 是栈顶，rsp-8是第一个可写区域，等价于：
    //  rsp -= 8
    // [rsp] = trampoline
    sp = (sp - 8) & ~uintptr_t(15); // SystemV ABI: 函数入口 rsp ≡ 8 (mod 16)
    *reinterpret_cast<uintptr_t *>(sp) =
        reinterpret_cast<uintptr_t>(&trampoline);
    f->ctx.rsp = sp;
}

/// @brief  register a new fiber running a function
/// @param fn function running on this fiber
void spawn(std::function<void()> fn) {
    Fiber *f = new Fiber();
    f->fn = std::move(fn);
    f->stack.resize(DEFAULT_STACK_SZ);

    _prepare_context(f);

    g_ready.push_back(f);
}

/// @brief  main loop of scheduler: picks the next fiber, and execute it
void run() {
    while (!g_ready.empty()) {
        Fiber *f = g_ready.front(); g_ready.pop_front();
        g_current = f; // to be read by trampoline

        switch_context(&g_sched_ctx, &f->ctx);
        
        // at this point, it is returned from fiber
        g_current = nullptr;
        if (f->finished) delete f;
        else g_ready.push_back(f);
    }
}

/// @brief yield execution flow back to scheduler
void yield() {
    switch_context(&g_current->ctx, &g_sched_ctx);
}


int main() {
    int x = 1; // var on stack of main(); the fiber should capture by reference

    spawn([&] {
        int local = 42;
        printf("fiber1: start, x=%d\n", x);
        yield();
        printf("fiber1: resumed, x=%d, local=%d\n", x, local);
        yield();
        printf("fiber1: done\n");
    });

    spawn([&] {
        printf("fiber2: start\n");
        yield();
        printf("fiber2: done\n");
    });

    printf("main: before run, x = %d\n", x);
    run();
    printf("main: after run, x = %d\n", x);
}