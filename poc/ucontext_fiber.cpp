// poc/ucontext_fiber.cpp - fiber prototype implemented via ucontext

#include <ucontext.h>
#include <cstdio>
#include <queue>
#include <functional>
#include <vector>

static const size_t DEFAULT_STACK_SZ = 64 * 1024;

struct Fiber {
    ucontext_t ctx; // register snapshot
    std::vector<char> stack; // fiber stack
    std::function<void()> fn;
    bool finished = false; // fiber state
};

// scheduler
static ucontext_t g_sched_ctx; // global scheduler context
static Fiber* g_current = nullptr; // running fiber
static std::deque<Fiber*> g_ready; // FIFO ready queue

// trampoline, for context switch
static void trampoline() {
    // fiber entry
    g_current->fn(); 
    // fiber yields / exit
    g_current->finished = true;
    // on return we should change uc_link to 'scheduler'
}

/// @brief  register a new fiber running a function
/// @param fn function running on this fiber
void spawn(std::function<void()> fn) {
    Fiber *f = new Fiber();
    f->fn = std::move(fn);
    f->stack.resize(DEFAULT_STACK_SZ);

    getcontext(&f->ctx); // allocate context
    f->ctx.uc_stack.ss_sp = f->stack.data(); // rsp -> stack top
    f->ctx.uc_stack.ss_size = f->stack.size();
    f->ctx.uc_link = &g_sched_ctx;  // return fn returns, it goes to scheduler
    makecontext(&f->ctx, trampoline, 0); // setup the fiber context

    g_ready.push_back(f);
}

/// @brief  main loop of scheduler: picks the next fiber, and execute it
void run() {
    while (!g_ready.empty()) {
        Fiber *f = g_ready.front(); g_ready.pop_front();
        g_current = f; // to be read by trampoline
        swapcontext(&g_sched_ctx, &f->ctx); // swap in with scheduler exeuction flow
        
        // at this point, it is returned from fiber
        g_current = nullptr;
        if (f->finished) delete f;
        else g_ready.push_back(f);
    }
}

/// @brief yield execution flow back to scheduler
void yield() {
    swapcontext(&g_current->ctx, &g_sched_ctx); // save it self, and goes back to scheduler
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