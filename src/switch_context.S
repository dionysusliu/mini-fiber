# x86-64 System V minimal context switch
# prototype: void switch_context(Context *from, Context *to)
#   rdi = from, rsi = to

    .text
    .globl switch_context
    .type switch_context, @function

switch_context:
    # save 'from' context (callee-saved registers + rsp; we would save return address at [rsp])
    movq %rbx, 0(%rdi)
    movq %rbp, 8(%rdi)
    movq %r12, 16(%rdi)
    movq %r13, 24(%rdi)
    movq %r14, 32(%rdi)
    movq %r15, 40(%rdi)
    movq %rsp, 48(%rdi)

    # resume 'to' context (rsp must be the last to resume, because it would switch the execution memory) 
    movq 0(%rsi), %rbx
    movq 8(%rsi), %rbp
    movq 16(%rsi), %r12
    movq 24(%rsi), %r13
    movq 32(%rsi), %r14
    movq 40(%rsi), %r15
    movq 48(%rsi), %rsp # switch stack
    ret # pop [rsp] to rip 
    .size switch_context, .-switch_context


    .section .note.GNU-stack,"",@progbits # tell the assembler: no execution stack for this
