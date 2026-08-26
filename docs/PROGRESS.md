# mini-fiber 进度记录

每次会话结束前更新。重要的概念性笔记单独放 `docs/` 下。

## 当前状态

- v0.1 第 0 步：ucontext 原型 — ✅ 完成（2026-08-25 输出验证通过，与推演一致）
- v0.1 第 1 步：自写 asm switch_context — ✅ 完成（2026-08-25 ping-pong 验证通过）
- v0.1 第 2 步：switch_context 接管调度 — ✅ 完成
  （2026-08-25，输出与第 0 步 diff 逐字节一致）
- v0.1 第 3 步：benchmark — ✅ 完成（2026-08-25，47× 差距，结果见下）
- **v0.2 里程碑达成**（2026-08-25，2.1 骨架 + 2.2 Priority/单测）
- **v0.3 里程碑达成**（2026-08-25，Baton + 统一入队原则，9 测试全绿）
- **v0.4 里程碑达成**（2026-08-26，RemoteMailbox + IdleDriver + 常驻
  TimerSource 借用模型，12 测试全绿 + TSan 干净）
- **Plan.md v0.1–v0.4 全部落地**；后续方向见 v0.4 节遗留清单
- `poc/` 放演示性代码，与正式 runtime 分开

## 决策记录

- 2026-08-25（v0.4 设计定稿，经 Folly/Boost/Argobots 三方对照）：
  ①"投递 ≠ 迁移"：外部线程只往线程安全邮箱投递 + poke，状态迁移
  （Blocked→Ready、awakened）全部由 runtime 线程在排空时执行——
  单线程不变量零改动。对照 Folly remoteReady（无锁 MPSC +
  LoopController）、Boost remote_ready（spinlock 队列 +
  algo notify）。
  ②邮箱与控制分离（接缝位置学 Folly）：RemoteMailbox 只管数据；
  IdleDriver 接口（park(deadline)/poke()）只管睡与醒。park 带
  deadline 一石二鸟：未来 EpollDriver（park=epoll_wait,
  poke=eventfd write）与 Boost 式调度器之钟共用此参数。
  ③防抖 notify：push 返回"之前是否为空"，仅空→非空时 poke
  （Folly insertHead 同款）。
  ④不学 Folly 控制反转：run() 永远是唯一循环主人，EventBase 式
  反转（LoopController::runLoop 回调 FM）是寄生别人 event loop
  的需求，mini-fiber 不需要。
  ⑤不学 Argobots pool 模式（pool 本身线程安全）：那会让每次
  awakened 付锁成本（含单线程快路径）。邮箱模式快路径零锁。
  mini-runtime 若做多线程调度，Argobots 模式重新成为候选。
  ⑥remote_wake 裸 Fiber* + 生命周期约定（调用方保证存活到排空；
  Folly 靠 FiberManager 拥有全部 Fiber 免除悬空），handle/refcount
  记遗留。定时器 v0.4 用 per-sleep std::thread（最简），
  Boost"调度器之钟"（sleep 队列 + park(deadline)）记为
  mini-runtime 方向。

- 2026-08-25（v0.3 设计确认，经 Boost/Folly 源码对照）：
  ①"变就绪者负责入队"统一原则（yield 自入队，run() 退出队列业务），
  对照 Folly readyFiber+preempt、Boost yield 进队尾文档语义；
  ②post 只入队不切换（Fiber::resume 仅入队，Boost awakened 同理）；
  ③铁律"状态登记先于 switch_context"（Folly setWaiter→preempt 原文）；
  ④Baton 三态 Init/Posted/Waiting = Folly 状态机单线程退化
  （其 atomic+futex+TIMEOUT/THREAD_WAITING 为多线程/超时扩展）；
  ⑤单 waiter 约定，违反时 assert（偷师 Folly 抛异常）；
  ⑥全阻塞死锁接受为已知限制；join/mutex/semaphore 不做。
  runtime 新增内部接口 detail::current/suspend_current/wake。

- 2026-08-25（v0.2 设计讨论）：Scheduler 采用 Boost.Fiber
  sched_algorithm 式**小接口**（awakened / pick_next /
  has_ready_fibers），调度循环留在 runtime；建立正式目录
  include/minifiber/ + src/，poc/ 保留为历史演示；v0.2 只做
  RoundRobin + Priority，其他策略（LIFO 等）留后续。
  附带决策：yield() 不碰队列——所有队列交互收敛在 run()/spawn()，
  原则"谁让 fiber 变就绪谁调 awakened"（为 v0.3 Baton 预留）；
  priority 作为 Fiber 字段，策略自由解读；runtime 全局状态收敛到
  fiber.cpp 匿名命名空间。

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

## v0.1 第 3 步：benchmark（✅ 完成 2026-08-25）

乒乓基准（每次迭代 = 往返 = 2 次切换），Release -O3 -march=native，
CMake FetchContent 引入 Google Benchmark v1.9.1（首次构建系统）。

实测（4×2.5GHz）：

| 指标 | switch_context | swapcontext | 倍差 |
|---|---|---|---|
| 往返 | 23.4 ns | 1109 ns | 47× |
| 单次切换 | ~11.7 ns（~29 cycles） | ~555 ns（~1387 cycles） | 47× |
| 每秒切换 | 86.0 M/s | 1.8 M/s | 47× |

发现与修正：

1. strace -c 实测 rt_sigprocmask ≈ 2 次/迭代 → **每次 swapcontext
   只 1 次 syscall**（SIG_SETMASK 第三参数非空时原子地存旧设新），
   修正此前"保存+恢复=2 次"的错误论断。数据纠正理论。
2. strace 计时膨胀 100×（112µs vs 1109ns，ptrace trap 开销）——
   strace 只用于计数，不用于测耗时。
3. 单次切换 11.7ns ≈ 29 cycles，接近 7 存 7 取的理论下限；
   对照内核线程切换 1–3µs，fiber 便宜两个数量级。

代码：CMakeLists.txt（仓库根，project 需声明 ASM 语言）、
poc/ctx_bench.cpp、SetItemsProcessed(2×iterations) 以"次"计数。

遗留：v0.1 功能达成（switch/stack/lifecycle/scheduler），
asm_pingpong.cpp 对齐旧写法仍未修（遗留 #1）。

## v0.2 步骤 2.1：正式骨架 + RoundRobinScheduler（✅ 完成 2026-08-25）

正式目录 include/minifiber/（context/fiber/scheduler/round_robin）
+ src/（fiber.cpp、switch_context.S 经 git mv 自 poc/）+
examples/demo_sched.cpp。CMake 新增 minifiber 库目标，bench_ctx
改为链库取符号。

验收：demo 输出与 v0.1 RoundRobin 语义一致；bench 回归 23.5ns/往返
（迁移无损）。poc/asm_pingpong、asm_fiber 的裸编译命令失效，属预期。

状态机：spawn→Ready；run 取出→Running；yield 归还由 run() 转
Ready 并 awakened；trampoline→Finished。
原则：谁让 fiber 变就绪，谁调 awakened（v0.3 Baton 沿用）。

下一步（2.2）：PriorityScheduler（R3 压力测试：核心四文件零改动）
+ GoogleTest 纯逻辑单测（分层红利：策略测试不需要栈与汇编）。

## v0.2 步骤 2.2：PriorityScheduler + 单测（✅ 完成 2026-08-25）

PriorityScheduler 用 multimap<int, Fiber*, greater<int>>（平级
FIFO、O(log n)，避开 priority_queue 比较器语义陷阱与堆序不稳定）。
R3 验收通过：fiber.hpp/context.hpp/fiber.cpp/switch_context.S 零改动。

tests/sched_test.cpp 5 个用例全绿——分层红利：策略测试不需要栈、
汇编与切换，退化为纯数据结构逻辑测试。examples/demo_priority
输出 high→mid→low（与 spawn 顺序相反，策略生效证据）。

踩坑：AI 给的测试代码漏了 using namespace minifiber（40 条级联
报错，根因只在第一条；GCC 的 did-you-mean 提示即诊断）。

## v0.3：Baton 与统一入队原则（✅ 完成 2026-08-25）

改动：State 增 Blocked；detail::current/suspend_current/wake 三内部
函数；yield 改为"自入队再切换"（统一原则：谁让 fiber 变就绪谁入队，
run() 退出队列业务只判 Finished）；Baton 三态 Init/Posted/Waiting，
post 先复位状态再 wake（防双重唤醒），wait 消费 Posted 立即返回
（顺序无关性）。

验收：demo_baton 两种顺序输出符合推演（阻塞路径 + 记忆消费路径）；
demo_sched 回归一致（yield 语义变化对外无损）；9 测试全绿
（5 调度 + 4 Baton，含 wake 只入队不抢占的队位断言、Baton 可重用）。

已知限制（设计决策）：单 waiter（Release 下 assert 空操作，要抓
误用需 Debug 构建）；全阻塞死锁时 run() 直接返回、fiber 泄漏；
join/mutex/semaphore 未实现（均为 Baton 直系后代，留给后续）。

## v0.4：Runtime Boundary（✅ 完成 2026-08-26）

交付：await.hpp/cpp（RemoteMailbox + IdleDriver 接口 +
CondVarIdleDriver + remote_wake + sleep_for）；run() 改造成
事件循环骨架（排空邮箱 → 运行 → park → 退出）；常驻 TimerSource
（multimap 最小堆 + 懒启动线程）；demo_await + 3 单测。

验收：12 测试全绿；demo_await 按 deadline 顺序唤醒、总时长 ~100ms
（park 非轮询）；TSan 零报告（跨线程共享面仅邮箱）；三个旧 demo 回归。

**借用模型（最终形态）**：事件源生命周期独立于 run()——run() 退出
只是"这批 fiber 跑完"，事件源不关停；真正的关停在静态析构
（EventSourcesFinalizer，同 TU 内构造逆序保证先于 mailbox 析构，
在飞 poke 必然落地后才销毁 cv）。对照 Folly：EventBase 属于应用，
FiberManager 只是借用；loop 多少次与事件源无关。

踩坑（两个真 bug，均已修复）：

1. **退出 race（TSan 抓获）**：per-sleep detach 线程的最后一声
   poke 可能落在静态析构销毁 cv 之后。本质是生命周期竞争
   （use-after-destroy），不是数据竞争——mutex 保护数据，
   保护不了对象的存在。修复历程：join 收割 → 常驻线程 + 注册表
   → 借用模型（最终）。
2. **stop/复用竞态导致 hang（gdb 抓获）**：中间版把
   shutdown_event_sources() 放在 run() 退出分支，gtest 多次 run()
   时第二次 run 的 schedule() 复用了已 stop 的 TimerSource——
   stopping_ 未复位，新线程起动即退出，runtime park 死等。
   教训：①"停止"要么可逆要么别停；②单次跑通不算验证
   （demo_await 单 run() 不触发，await_test 多 run() 必触发）；
   ③根因是"run() 退出 ≠ runtime 关停"的语义混淆，借用模型
   从设计上消灭了这个 bug 类别，且净删代码（注册表、复位 hack、
   run 里的关停调用全部移除）。

遗留（mini-runtime 方向）：EpollIdleDriver（park=epoll_wait,
poke=eventfd，顺手获得跨进程 poke 能力）；调度器之钟（Boost 式
sleep 队列，park(deadline) 参数已预留）；remote_wake 的
handle/refcount；取消语义；多线程调度（Argobots pool 模式重新
成为候选）。跨进程邮箱（shm+进程共享锁 / mqueue / ZMQ）、RDMA
集成（completion fd 可 epoll、wr_id 即句柄）已做概念推演，
IdleDriver 接口均无需改动。

## v0.1 总结

第 0 步 ucontext 定型 → 第 1 步 20 行汇编替换原语 → 第 2 步接管
调度（输出逐字节一致）→ 第 3 步量化 47×。方法论沉淀：黑盒先行、
原语单独验证、逐字节 diff 验收、strace 只计数。
