# AGENTS.md

本文件为 AI agent 在本仓库工作时提供指导。使用中文交流。

## 项目现状与关键文档

- 路线图与版本目标（v0.1–v0.4）：`docs/Plan.md`
- 参考资料索引、按设计问题查表：`docs/References.md`
- 进度记录：`docs/PROGRESS.md`，首个动手步骤时创建并持续更新
- 平台约束：仅 Linux x86-64 / System V AMD64 ABI
- 当前为规划期，无源码；不要提前生成项目骨架，目标细化需先与用户确认

## 学习/教学导向（重要）

本项目以学习、教学为首要目标，协作时遵守：

1. **做好记录**：每一步的成果、原理、遗留问题记录到
   `docs/PROGRESS.md`，每次会话结束前更新，供下次接续。
   重要的概念性笔记, 单独放 `docs/` 下。
2. **step-by-step 推进**：一次只做一个最小可验证的步骤，
   跑通、观察、记录后再进入下一步；不跳步、不提前堆砌功能。
3. **先原理后编码**：每一步先解释涉及的 Linux 原理
   （syscall、内核行为、数据结构），再讨论编码方式
   （怎么写、为什么这么写），最后才动手。
4. 演示性代码（验证原理用）与正式代码（runtime 本体）可以分开，
   不必过早追求工程结构，但每次提交的意图要清楚。
5. **代码讲解格式**：需要给出代码时，按此结构组织——
   先列出本步的全部改动点与整体思路；然后逐个小步展开，
   每个小步先说"要做什么、为什么"，再列对应代码；
   最后给出完整可组装的代码、验证命令和预期观察点。
6. **编辑分工（强制）**：AI 手把手教用户怎么做——给出思路、步骤、
   验证命令和预期观察点；**可以在对话中展示代码片段**供用户参考，
   也可以指导用户使用自动化工具（cmake、gdb、perf 等命令）。
   但 **AI 绝不替用户生成或修改任何代码文件和配置文件**
   （.cpp/.hpp/CMakeLists.txt 等），即使用户说"帮我改/帮我写"，
   也只输出内容让用户自己落盘。例外：AGENTS.md 和 `docs/` 下的
   文档记录可以由 AI 直接编辑。
   用户改完贴报错，AI 协助诊断。

## 工具链决策（草案，待与用户确认）

| 用途 | 工具 | 说明 |
|---|---|---|
| 构建 | CMake | C++ 事实标准；多 build 目录并存（Debug/ASan 与 Release），方便调试与性能对比互不干扰 |
| 单测 | GoogleTest | 经 CMake FetchContent 引入，不手动安装 |
| 基准 | Google Benchmark | 唯一性能测量工具；进程内测量 ns/op，同一二进制内注册多个变体同环境对比（如 context switch 耗时） |
| 调试 | gdb | `layout reg` / `info registers` 观察 RSP/RIP/callee-saved 寄存器，验证 context switch 行为 |
| 竞争检查 | TSan | 本项目初期为单线程协作调度，不需要；引入多线程后再启用 |
| 内存检查 | ASan + UBSan | 内存错误与未定义行为；注意下方 ASan fiber 陷阱 |
| 性能分析 | perf | 看 cache miss、分支预测失败，解释"为什么优化有效" |

不引入 hyperfine：它是进程级计时，启动开销和噪声会混入结果，
Google Benchmark 的进程内测量粒度更细、信噪比更高。

性能测量一律用 Release 构建（`-O3 -march=native`），Debug 只用于调试。

ASan 陷阱：ASan 不认识 fiber stack，context switch 前后必须配对调用
`__sanitizer_start_switch_fiber` / `__sanitizer_finish_switch_fiber`，
否则会大量误报 stack 相关错误。

