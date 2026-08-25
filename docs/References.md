# mini-fiber References

本文件用于在遇到具体设计问题时快速定位资料；不要求按项目顺序通读所有实现。

## 1. Context Switch / ABI

### System V AMD64 ABI

- [x86-64 psABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- 用于理解：caller-saved / callee-saved registers、stack layout、stack alignment、function call / return 约定。
- 实现 `switch_context()` 前先确认：哪些寄存器必须保存、进入 fiber entry 时 stack 是否满足 ABI alignment。

### Boost.Context

- [Boost.Context 文档](https://www.boost.org/doc/libs/latest/libs/context/doc/html/index.html)
- [fcontext / fiber context 说明](https://www.boost.org/doc/libs/latest/libs/context/doc/html/context/ff.html)
- 重点看 x86-64 的 context 保存与跳转思路。把它当工业级参考，不直接照抄；先从 ABI 推导自己的最小实现。

## 2. Fiber Runtime / Scheduling

### Boost.Fiber

- [Boost.Fiber 文档](https://www.boost.org/doc/libs/latest/libs/fiber/doc/html/index.html)
- 重点：Fiber 生命周期、round-robin scheduling、scheduler customization、同步原语。
- 它适合作为本项目整体结构的主要参考：`Context → Fiber → Scheduler → Ready Queue`。

### Folly Fibers

- [folly/fibers](https://github.com/facebook/folly/tree/main/folly/fibers)
- 重点阅读：`Fiber`、`FiberManager`、`LoopController`、`Baton`。
- 关注 production runtime 如何将 fiber scheduler 接入 event loop；不需要从头到尾通读。

### Argobots

- [Argobots](https://www.argobots.org/)
- [Argobots GitHub repository](https://github.com/pmodels/argobots)
- 用于横向理解 user-level threading、execution stream、pool 与 scheduler policy 的分层；重点是设计边界，不是 API 模仿。

## 3. Historical / API Reference

### POSIX `ucontext`

- [`getcontext` / `setcontext` / `makecontext` / `swapcontext`](https://man7.org/linux/man-pages/man3/swapcontext.3.html)
- 用于理解“显式保存 execution context + 独立 stack + trampoline”的基本模型。
- 它已被 POSIX.1-2008 移除，不把它作为最终实现依赖；可用来验证概念或写最早期原型。

## Recommended Reading Order

1. System V AMD64 ABI：先理解调用约定与 stack alignment。
2. Boost.Context：将 ABI 规则映射到最小 context switch。
3. POSIX `ucontext`：建立独立 stack 与 entry/trampoline 的直觉。
4. Boost.Fiber：完成 Fiber、ready queue、round-robin scheduler 与同步原语。
5. Folly Fibers：研究 event loop integration 与 `Baton`。
6. Argobots：横向比较 scheduler / pool / execution stream 的设计取舍。

## 按设计问题横向研究

遇到问题时，优先按问题查，而不是按库从头读：

| 设计问题 | 优先资料 | 要回答的问题 |
| --- | --- | --- |
| context 要保存什么？ | System V AMD64 ABI、Boost.Context | 哪些寄存器是 callee-saved？如何恢复 RIP / RSP？ |
| 新 fiber 如何启动？ | `ucontext`、Boost.Context | 如何布置初始 stack、entry 与 trampoline？ |
| `yield()` 如何回到 scheduler？ | Boost.Fiber | 当前 Fiber 与 scheduler context 如何互换？ |
| ready queue 如何抽象？ | Boost.Fiber、Argobots | policy 与 Fiber 状态如何分离？ |
| 等待与唤醒如何保证正确？ | Folly `Baton`、Boost.Fiber | `BLOCKED → READY` 由谁执行？竞态边界在哪里？ |
| 如何接入事件循环？ | Folly Fibers | event 完成后如何安全地 wake fiber？ |

阅读源码时，先写下当前问题与需要验证的 invariant；只追踪到能回答该问题的调用路径即可。

