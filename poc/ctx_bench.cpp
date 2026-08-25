#include <benchmark/benchmark.h>
#include <ucontext.h>
#include <cstdint>

struct Context { uint64_t rbx, rbp, r12, r13, r14, r15, rsp; };
extern "C" void switch_context(Context* from, Context* to);

// ---------- 自写 asm 版 ----------
static Context g_ctx_main, g_ctx_a;
static char g_stack_a[64 * 1024];    // 软件对齐已兜底，缓冲区无需 alignas

static void bounce_asm() {
    for (;;) switch_context(&g_ctx_a, &g_ctx_main);   // 唯一动作：切回去
}

static void BM_switch_context(benchmark::State& state) {
    uintptr_t sp = reinterpret_cast<uintptr_t>(g_stack_a + sizeof g_stack_a);
    sp = (sp - 8) & ~uintptr_t(15);   // 对齐修复写法（见 docs/StackAlignment.md）
    *reinterpret_cast<uintptr_t*>(sp) =
        reinterpret_cast<uintptr_t>(&bounce_asm);
    g_ctx_a.rsp = sp;

    for (auto _ : state)
        switch_context(&g_ctx_main, &g_ctx_a);
    state.SetItemsProcessed(2 * state.iterations());   // 以"次切换"计数
}
BENCHMARK(BM_switch_context);

// ---------- ucontext 版 ----------                                                   
static ucontext_t g_u_main, g_u_a;
static char g_stack_u[64 * 1024];

static void bounce_u() {
    for (;;) swapcontext(&g_u_a, &g_u_main);
}

static void BM_swapcontext(benchmark::State& state) {
    getcontext(&g_u_a);               // makecontext 前必须先初始化快照
    g_u_a.uc_stack.ss_sp = g_stack_u;
    g_u_a.uc_stack.ss_size = sizeof g_stack_u;
    g_u_a.uc_link = &g_u_main;        // bounce 永不返回，uc_link 仅形式
    makecontext(&g_u_a, bounce_u, 0);

    for (auto _ : state)
        swapcontext(&g_u_main, &g_u_a);
    state.SetItemsProcessed(2 * state.iterations());                                   
}
BENCHMARK(BM_swapcontext);

BENCHMARK_MAIN();