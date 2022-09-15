# Copyright (c) 2018-2020 The EFramework Project
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# 定义常量
# 协程栈上界
.equ FIBER_STACK_UPPER_OFFSET, 16
# 协程栈指针
.equ FIBER_STACK_PTR_OFFSET, 32
# 协程状态
.equ FIBER_STATUS_OFFSET, 40
# 父协程
.equ FIBER_PARENT_OFFSET, 48
# 协程调度器
.equ FIBER_SCHED_OFFSET, 56

# 当前调度的协程
.equ SCHED_CURRENT_FIBER_OFFSET, 0

# 线程状态：退出
.equ FIBER_STATUS_EXITED, 0
# 协程状态：初始化
.equ FIBER_STATUS_INITED, 1

# 告诉编译器后续跟的是一个全局可见的名字【可能是变量，也可以是函数名】
.global ef_fiber_internal_swap
.global ef_fiber_internal_init

# 后续编译出来的内容放在代码段【可执行】
.text

ef_fiber_internal_swap:
# rdx: to_yield
# to_yield标识保存在rax中，作为返回值返回
mov %rdx,%rax
# 保存一堆寄存器的值，保存现场，这个是保存在当前协程栈中
push %rbx
push %rbp
push %rsi
push %rdi
push %r8
push %r9
push %r10
push %r11
push %r12
push %r13
push %r14
push %r15
# 压入rflags寄存器的值，8个字节
pushfq
# rsi: &current->stack_ptr
# 将当前协程的栈指针rsp存储到当前协程的stack_ptr处(为了之后将栈切换回来，切换回来之后，栈指针指向之前最后保存的r15寄存器的内容，然后会从popfq指令开始执行，因为执行切换栈的指令之后，rip是指向popfq指令的，之后正好从栈中弹出之前保存的一些寄存器)
mov %rsp,(%rsi)
# rdi: to->stack_ptr
# 将要切换执行的协程的栈指针送到rsp中，完成栈切换
mov %rdi,%rsp
_ef_fiber_restore:
# 执行完切栈动作之后，后面执行的指令都是在需要被执行的协程栈中
# 从需要被执行的协程栈中弹出一些值到寄存器，当前栈指针就是stack_ptr
popfq
pop %r15
pop %r14
pop %r13
pop %r12
pop %r11
pop %r10
pop %r9
pop %r8
# rdi: param or fiber
pop %rdi
# rsi: 0
pop %rsi
# rbp: stack_upper
pop %rbp
# rbx: 0
pop %rbx
# ret指令执行时，从栈中弹出8个字节到指令指针寄存rip，正好是fiber_proc的函数地址，之后开始执行fiber_proc函数，参数就是rdi中的param
ret

_ef_fiber_exit:
pop %rdx
mov $FIBER_STATUS_EXITED,%rcx
mov %rcx,FIBER_STATUS_OFFSET(%rdx)
mov FIBER_PARENT_OFFSET(%rdx),%rcx
mov FIBER_SCHED_OFFSET(%rdx),%rdx
mov %rcx,SCHED_CURRENT_FIBER_OFFSET(%rdx)
mov FIBER_STACK_PTR_OFFSET(%rcx),%rsp
jmp _ef_fiber_restore

ef_fiber_internal_init:
mov $FIBER_STATUS_INITED,%rax
mov %rax,FIBER_STATUS_OFFSET(%rdi)
mov %rdi,%rcx
mov FIBER_STACK_UPPER_OFFSET(%rdi),%rdi
mov %rcx,-8(%rdi)
# 取有效地址，也就是取偏移地址，即C语言中的取地址符&
# leaq a(b, c, d), %rax
# leaq a(b, c, d), %rax 先计算地址a + b + c * d，然后把最终地址载到寄存器rax中
# 即算出_ef_fiber_exit函数首条指令的地址传送到rax寄存器中
lea _ef_fiber_exit(%rip),%rax
mov %rax,-16(%rdi)
mov %rsi,-24(%rdi)
xor %rax,%rax
mov %rax,-32(%rdi)
mov %rdi,-40(%rdi)
mov %rax,-48(%rdi)
mov %rdx,-56(%rdi)
mov %rax,-64(%rdi)
mov %rax,-72(%rdi)
mov %rax,-80(%rdi)
mov %rax,-88(%rdi)
mov %rax,-96(%rdi)
mov %rax,-104(%rdi)
mov %rax,-112(%rdi)
mov %rax,-120(%rdi)
mov %rax,-128(%rdi)
mov %rdi,%rax
sub $128,%rax
ret

