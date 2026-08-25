# mini-fiber 进度记录

每次会话结束前更新。重要的概念性笔记单独放 `docs/` 下。

## 当前状态

- v0.1 第 0 步：ucontext 原型 — ✅ 完成（2026-08-25 输出验证通过，与推演一致）
- v0.1 第 1 步：自写 asm switch_context — ✅ 完成（2026-08-25 ping-pong 验证通过）
- v0.1 第 2 步：switch_context 接管调度 — ✅ 完成
  （2026-08-25，输出与第 0 步 diff 逐字节一致）
- 下一步：第 3 步，Google Benchmark 量化 swapcontext vs switch_context
- `poc/` 放演示性代码，与正式 runtime 分开

## 决策记录

- 2026-08-25：语言定为 C++（C++20）。核心 `switch_context` 用独立 `.S`
  汇编 + `extern "C"` 接入；选择 C++ 的理由：RAII 管理 fiber 栈生命周期、
  GoogleTest/GoogleBenchmark 原生、后续 mini-runtime 项目沿用。
- 2026-08-25：起点走 ucontext POC——先把调度逻辑定型，再写汇编，
  避免"栈布局"与"汇编正确性"两个未知数同时调试。
- 2026-08-25：最小交付物定义 = 跑通
  `spawn(入队) / run(FIFO 调度循环) / yield(交回控制权)`，
  fiber 局部变量跨 yield 完好，round-robin 交错输出。

## v0.1 第 0 步：ucontext 原型（进行中）

目标：用 `getcontext/makecontext/swapcontext` 实现 spawn/yield/run，
建立"上下文 = 寄存器快照 + 栈"的心智模型。

关键原理（详见对话记录）：

- `makecontext` 只**布置**快照（改 RIP/RSP 指向新栈和 trampoline），
  不发生切换；调用前必须先 `getcontext` 初始化，否则快照是垃圾。
- `uc_link` 决定 fiber 函数 return 后的去向；NULL 会导致整个线程退出。
- makecontext 的变参按 `int` 传递，无法直接传 `Fiber*` 指针——
  用全局 `g_current`（切换前写入、trampoline 启动时读取）传递。
- `swapcontext` 内含 sigprocmask 保存/恢复（两次 syscall），是它慢的
  主因；asm 版砍掉这一段（单线程无信号场景）。
- FINISHED 后由 scheduler 回收 fiber 栈，fiber 自己不碰
  （不能释放自己正踩着的栈）。

代码：`poc/ucontext_fiber.cpp`（AI 给出，用户落盘）。
验证：round-robin 交错输出、fiber 局部变量跨 yield 完好、
gdb 观察 fiber 栈与 main 栈是两个不相交的地址区间。

遗留 / 下一步：

1. ~~落盘编译运行~~ — 已完成，输出与推演一致
2. gdb 观察寄存器与栈地址（可结合第 1 步一起做）
3. 第 1 步：自写 asm `switch_context`（见下）
4. 之后：Google Benchmark 测 ucontext vs 自写 asm 切换耗时差距

## v0.1 第 1 步：自写 asm switch_context（✅ 完成 2026-08-25）

ping-pong 验证通过：main 栈与 g_stack 两个世界来回切换、
fiber 局部变量跨切换完好。

目标：约 40 行 x86-64 汇编替换 swapcontext。先单独验证原语
（ping-pong demo），再接入 fiber demo。

关键原理：

- ABI 把寄存器分为 caller-saved（rax rcx rdx rsi rdi r8-r11）与
  callee-saved（rbx rbp r12-r15）。switch_context 是普通函数，
  编译器在 call 边界已把活跃的 caller-saved 值 spill——所以只需
  保存 callee-saved 6 个 + rsp，共 7 个槽。
- rip 不用存：call 已把返回地址压在 [rsp]，保存 rsp 就保存了
  "回来后从哪继续"；恢复时 ret 弹出即完成跳转。
- 新上下文制造：栈顶 16 对齐，写入入口函数地址，saved rsp 指向它。
  对齐推导：ret 弹出后 rsp = R+8，须满足 ABI"入口 rsp ≡ 8 (mod 16)"
  → R ≡ 0 (mod 16)。差 8 字节的经典症状是 printf 内 movaps 崩溃。
- 相比 swapcontext 砍掉了 sigprocmask（信号掩码保存/恢复）——
  单线程无信号场景安全，也是它快的主因。

代码：`poc/switch_context.S`、`poc/asm_pingpong.cpp`（AI 给出，用户落盘）。
验证：main 栈与 fiber 栈地址不相交、来回各两次、无崩溃。✅ 已验证。

踩坑记录（教训沉淀）：

1. GCC 12 下 `static alignas(16) char ...` 报"standard attributes in
   middle of decl-specifiers"。规则：alignas 只能放声明最前或
   declarator 后（`static char g_stack[N] alignas(16);`）。
   本 demo 里其实可省——`sp &= ~15` 已在软件层对齐初始 RSP。
2. `.S` 恢复段 7 个 movq 偏移复制粘贴后忘改，全为 0(%rsi)：
   rsp 从 rbx 槽加载 → 必然 SIGSEGV。汇编器不做偏移语义检查，
   offset 与 struct 字段的对应是"人肉 ABI"。
   防错：gdb `p &((Context*)0)->rsp` 或 `offsetof` 对照；
   `objdump -d | grep -A15 switch_context` 复查。
   约定：Context 以后只能尾部追加字段，禁止重排/中间插入。

3. 初始栈对齐写法边界 bug（详见 docs/StackAlignment.md）：原
   `sp &= ~15` 写 [R] 在 top 恰好 16 对齐时越界 8 字节
   （operator new 保证 16 对齐 + 64K 是 16 倍数，必然触发）。
   ping-pong 未崩纯属相邻存储恰好无害；ASan 下为真阳性。
   修复：`sp = (sp - 8) & ~uintptr_t(15);`
   教训：对齐计算要同时保证"值对齐"与"写入不越界"两个不变量。

## v0.1 第 2 步：switch_context 接管 fiber 调度（✅ 完成 2026-08-25）

`poc/asm_fiber.cpp` 复用 `switch_context.S`，五处改动：
Context 替代 ucontext_t；三行伪造初始栈替代 getcontext/makecontext
（提炼为 `_prepare_context`，含对齐修复写法）；trampoline 末尾
自切回替代 uc_link；yield/run 改调 switch_context；无 sigprocmask。

关键简化：getcontext 整个消失——运行中的上下文在切走瞬间被
switch_context 捕获，只有初始状态需要手工制造；g_sched_ctx
零初始化即可（首次切走时才被填充）。

验收：输出与第 0 步 ucontext 版 diff 逐字节一致；gdb 确认
trampoline 处 $rsp = top − 16（对齐修复生效）、多个 fiber
各自独立栈区间。

遗留：

1. `poc/asm_pingpong.cpp` 的对齐行仍是旧写法（越界 bug 还在），
   下次触碰该文件时顺手修复
2. `poc/asm_fiber.cpp` 首行注释仍是 "ucontext"（复制残留）

下一步（第 3 步）：Google Benchmark 量化 swapcontext vs
switch_context 耗时差距（Release `-O3` 构建）。
