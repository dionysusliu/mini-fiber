# 栈对齐笔记：新 fiber 初始栈为什么要对齐

记录 v0.1 第 1/2 步中初始栈制造代码的完整理由，以及由此发现的
边界 bug。推导基于 SysV AMD64 ABI 与 x86-64 硬件语义。

## 问题背景

制造新 fiber 的初始上下文 = 伪造"这个函数曾经被 call 过"的栈状态：
把入口函数地址写在一个槽 R 上，令 `ctx.rsp = R`，
之后 `switch_context` 末尾的 `ret` 弹出 [R] 完成首跳。

初始栈制造需要同时满足两个不变量：

1. 值对齐：R ≡ 0 (mod 16)
2. 不越界：R + 8 ≤ top（写入 [R] 的 8 字节落在缓冲区内）

## 推导链：为什么 R ≡ 0 (mod 16)

1. switch_context 恢复上下文：`mov R, %rsp; ret`
2. ret 弹出 [R] 跳转，目标函数入口处 rsp = R + 8
3. SysV AMD64 ABI 规定：函数入口 rsp ≡ 8 (mod 16)
   （等价说法：rsp + 8 是 16 的倍数）
4. 联立：R + 8 ≡ 8 (mod 16) → R ≡ 0 (mod 16)

正常运行中的函数天然满足第 3 条：编译器保证 call 前 rsp ≡ 0，
call 压入返回地址后入口 rsp ≡ 8。伪造初始栈必须手工达到同一不变量。

## 为什么 ABI 规定 16 字节对齐

SSE 指令（如 movaps）读写 16 字节操作数时要求地址 16 字节对齐，
否则触发 #GP 段错误。编译器依赖"入口 rsp ≡ 8"不变量，才能确定性地
把局部变量放在对齐位置并用对齐 SSE 指令访问。

## 症状特征（错 8 字节时）

- 简单代码一切正常（只用通用寄存器时对齐无关紧要）
- 某天崩在 printf 等 libc 深处的 movaps，回溯看不出与自己代码有关
- stackful fiber 的经典坑，与调度逻辑无关，极难定位

## 边界 bug（2026-08-25 复查发现）

原写法：

```cpp
uintptr_t sp = (uintptr_t)(stack_top);
sp &= ~uintptr_t(15);              // R ≡ 0 (mod 16) ✓
*(uintptr_t*)sp = entry_addr;      // R + 8 ≤ top ？✗
```

当 top 本来就 16 对齐时（operator new 保证 16 对齐、
64K 是 16 的倍数——本项目场景必然如此），R = top = one-past-end，
[R] 越界写 8 字节。ping-pong demo 没炸是因为那 8 字节恰好落进
相邻静态对象/分配器 slack 的无关位置；ASan 下是真阳性的
heap-buffer-overflow。

正确写法（两个不变量同时满足）：

```cpp
sp = (sp - 8) & ~uintptr_t(15);    // R ≡ 0 (mod 16) 且 R + 8 ≤ top
```

- top ≡ 0 时：R = top - 16，[R] 落在缓冲区内最后 16 字节处
- top 不对齐时：向下取整自然留出余量
- 入口不变量不变：ret 后 rsp = R + 8 仍 ≡ 8 (mod 16)

## 位运算备忘

- 15 = 0b1111（低 4 位掩码）；`&= ~15` 清零低 4 位 = 向下取整到 16 倍数
- 必须向下、不能向上：向上取整可能越过 top 落到缓冲区外
- gdb 验证：`p sp % 16` 应为 0；`p top - sp` 应 ≥ 8

## 相关代码

- `poc/asm_pingpong.cpp`：初始栈制造（含此 bug，修复方案如上）
- `poc/asm_fiber.cpp`：spawn 中的同一段（创建时即用正确写法）
- `poc/switch_context.S` 的 ret：推导链的另一半
