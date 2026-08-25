# mini-fiber Plan

## Goal

实现一个最小的 C++ stackful fiber runtime，用于理解：

- user-level context switch
- fiber stack 与生命周期
- cooperative scheduling
- fiber synchronization
- custom scheduling policy

第一阶段只支持 Linux x86-64 / System V AMD64 ABI。

## v0.1 — Minimal Fiber

目标：跑通最小 cooperative fiber。

### Context Switch

实现：

```cpp
switch_context(Context* from, Context* to);
```

理解并处理：

- callee-saved registers
- stack pointer
- instruction / return address
- stack alignment

使用少量 x86-64 assembly 自己实现，不依赖 Boost.Context。

### Fiber Stack

每个 Fiber 拥有独立 stack。第一版使用固定大小 stack；后续再考虑：

- `mmap`
- guard page
- stack pool

### Fiber Lifecycle

最小状态：

```text
READY → RUNNING → READY
          │
          └────→ FINISHED
```

实现 fiber entry / trampoline，并正确处理 fiber function return。

### Cooperative Scheduler

提供最小 API：

```cpp
spawn(fn);
yield();
run();
```

使用单线程 FIFO / round-robin ready queue。

## v0.2 — Scheduler Abstraction

将 scheduling policy 与 Fiber 分离：

```text
Fiber
  ↓
Scheduler
  ↓
Ready Queue
```

首先实现 `RoundRobinScheduler`，然后实验：

- `PriorityScheduler`
- user-defined scheduling policy

## v0.3 — Fiber Synchronization

实现最小等待/唤醒机制：

```cpp
Baton::wait();
Baton::post();
```

增加状态：

```text
READY
RUNNING
BLOCKED
FINISHED
```

在 `Baton` 基础上再考虑：

- `join`
- mutex
- condition variable
- semaphore

## v0.4 — Runtime Boundary

研究 Fiber 如何与外部事件系统结合：

```text
Fiber → wait → Scheduler → external event → wake → READY
```

只完成接口和机制验证，不实现完整 async IO runtime。`io_uring` / `epoll` integration 留给后续 `mini-runtime` 项目。

## Out of Scope

当前不实现：

- preemptive scheduling
- multi-thread scheduling
- work stealing
- C++20 stackless coroutine
- production-grade stack allocator
- complete async IO runtime

