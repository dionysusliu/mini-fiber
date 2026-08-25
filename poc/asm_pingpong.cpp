// check if switch_context.S works

#include <cstdint>
#include <cstdio>

struct Context {
    uint64_t rbx, rbp, r12, r13, r14, r15, rsp;
};

extern "C" void switch_context(Context *from, Context *to);

static Context g_main_ctx;
static Context g_fiber_ctx;
alignas(16) static char g_stack[64 * 1024];


static void fiber_fn() {
    int local = 42;
    printf("fiber: first run, &local=%p\n", (void*)&local);
    switch_context(&g_fiber_ctx, &g_main_ctx); // switch back to main
    printf("fiber: resumed, local=%d\n", local); // local var are reserved
    switch_context(&g_fiber_ctx, &g_main_ctx); // switch back to main
}

int main() {
    int local = 1;
    // prepare fiber_fn's stack (like it has been called)
    uintptr_t sp = reinterpret_cast<uintptr_t>(g_stack + sizeof g_stack);
    sp &= ~uintptr_t(15);
    *reinterpret_cast<uintptr_t*>(sp) =
            reinterpret_cast<uintptr_t>(&fiber_fn);    // [R] = 入口地址，ret 会跳它
    g_fiber_ctx.rsp = sp;

    printf("main  : before switch, &local=%p\n", (void*)&local);
    switch_context(&g_main_ctx, &g_fiber_ctx);     // 切过去
    printf("main  : back from fiber\n");
    switch_context(&g_main_ctx, &g_fiber_ctx);     // 再切过去
    printf("main  : done\n");
}