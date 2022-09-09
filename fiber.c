// Copyright (c) 2018-2020 The EFramework Project
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include "fiber.h"

static long ef_page_size = 0;
static ef_fiber_sched_t *ef_fiber_sched = NULL;

long ef_fiber_internal_swap(void *new_sp, void **old_sp_ptr, long retval);

void *ef_fiber_internal_init(ef_fiber_t *fiber, ef_fiber_proc_t fiber_proc, void *param);

ef_fiber_t *ef_fiber_create(ef_fiber_sched_t *rt, size_t stack_size, size_t header_size, ef_fiber_proc_t fiber_proc, void *param)
{
    ef_fiber_t *fiber;
    void *stack;
    long page_size = ef_page_size;

    if (stack_size == 0) {
        stack_size = (size_t)page_size;
    }

    /*
     * make the stack_size an integer multiple of page_size
     */
    // 将自定义的栈大小，对齐到内存页的大小（不足一页的，将其置为一页的大小，大于一页的有不满一页的补齐）
    // stack_size最小为一个页
    stack_size = (size_t)((stack_size + page_size - 1) & ~(page_size - 1));

    /*
     * reserve the stack area, no physical pages here
     */
    /**
     * 内存映射，简而言之就是将用户空间的一段内存区域映射到内核空间，映射成功后，用户对这段内存区域的修改可以直接反映到内核空间，同样，内核空间对这段区域的修改也直接反映用户空间。那么对于内核空间<---->用户空间两者之间需要大量数据传输等操作的话效率是非常高的。
     * void * mmap(void *start, size_t length, int prot , int flags, int fd, off_t offset)
     * start：要映射到的内存区域的起始地址，通常都是用NULL（NULL即为0）。NULL表示由内核来指定该内存地址
     * prot：期望的内存保护标志，不能与文件的打开模式冲突。是以下的某个值，可以通过or运算合理地组合在一起
            PROT_EXEC //页内容可以被执行
            PROT_READ  //页内容可以被读取
            PROT_WRITE //页可以被写入
            PROT_NONE  //页不可访问

        flags：指定映射对象的类型，映射选项和映射页是否可以共享。它的值可以是一个或者多个以下位的组合体
            MAP_FIXED ：使用指定的映射起始地址，如果由start和len参数指定的内存区重叠于现存的映射空间，重叠部分将会被丢弃。如果指定的起始地址不可用，操作将会失败。并且起始地址必须落在页的边界上。
            MAP_SHARED ：对映射区域的写入数据会复制回文件内, 而且允许其他映射该文件的进程共享。
            MAP_PRIVATE ：建立一个写入时拷贝的私有映射。内存区域的写入不会影响到原文件。这个标志和以上标志是互斥的，只能使用其中一个。
            MAP_DENYWRITE ：这个标志被忽略。
            MAP_EXECUTABLE ：同上
            MAP_NORESERVE ：不要为这个映射保留交换空间。当交换空间被保留，对映射区修改的可能会得到保证。当交换空间不被保留，同时内存不足，对映射区的修改会引起段违例信号。
            MAP_LOCKED ：锁定映射区的页面，从而防止页面被交换出内存。
            MAP_GROWSDOWN ：用于堆栈，告诉内核VM系统，映射区可以向下扩展。
            MAP_ANONYMOUS ：匿名映射，映射区不与任何文件关联。
            MAP_ANON ：MAP_ANONYMOUS的别称，不再被使用。
            MAP_FILE ：兼容标志，被忽略。
            MAP_32BIT ：将映射区放在进程地址空间的低2GB，MAP_FIXED指定时会被忽略。当前这个标志只在x86-64平台上得到支持。
            MAP_POPULATE ：为文件映射通过预读的方式准备好页表。随后对映射区的访问不会被页违例阻塞。
            MAP_NONBLOCK ：仅和MAP_POPULATE一起使用时才有意义。不执行预读，只为已存在于内存中的页面建立页表入口。

        fd：文件描述符（由open函数返回）
        offset：表示被映射对象（即文件）从那里开始对映，通常都是用0。 该值应该为大小为PAGE_SIZE的整数倍

        成功执行时，mmap()返回被映射区的指针，munmap()返回0。失败时，mmap()返回MAP_FAILED[其值为(void *)-1]，munmap返回-1。errno被设为以下的某个值
            EACCES：访问出错
            EAGAIN：文件已被锁定，或者太多的内存已被锁定
            EBADF：fd不是有效的文件描述词
            EINVAL：一个或者多个参数无效
            ENFILE：已达到系统对打开文件的限制
            ENODEV：指定文件所在的文件系统不支持内存映射
            ENOMEM：内存不足，或者进程已超出最大内存映射数量
            EPERM：权能不足，操作不允许
            ETXTBSY：已写的方式打开文件，同时指定MAP_DENYWRITE标志
            SIGSEGV：试着向只读区写入
            SIGBUS：试着访问不属于进程的内存区



        int munmap(void *start, size_t length)
        start：要取消映射的内存区域的起始地址
        length：要取消映射的内存区域的大小。
        成功执行时munmap()返回0。失败时munmap返回-1.
     * */
     // 相当于申请了一段stack_size大小的内存，设置这段内存为不可访问，当协程栈溢出到不可访问的地方会触发SIGSEGV信号，从而被我们捕获然后扩展协程栈，起始就是将这块不可访问的内存的一部分设置为可读可写
    stack = mmap(NULL, stack_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (MAP_FAILED == stack) {
        return NULL;
    }

    /*
     * map the highest page in the stack area
     */
    // 预留出stack内存中最最高位地址的一个页，将其设置为可读可写，即先使用这一个页
    if (mprotect((char *)stack + stack_size - page_size, page_size, PROT_READ | PROT_WRITE) < 0) {
        munmap(stack, stack_size);
        return NULL;
    }

    /*
     * the topmost header_size bytes used by ef_fiber_t and
     * maybe some outter struct which ef_fiber_t nested in
     */
    // 从上面划分出来的可读可写的一个页中，再划分出来ef_fiber_t结构体的内存
    fiber = (ef_fiber_t*)((char *)stack + stack_size - header_size);
    // 设置ef_fiber_t结构的一些属性（主要是协程栈的位置）
    fiber->stack_size = stack_size; // 协程栈最大大小（目前有部分是不可访问的内存）
    fiber->stack_area = stack;  // 协程栈最大的栈底（目前有部分是不可访问的内存）
    fiber->stack_upper = (char *)stack + stack_size - header_size;  // 协程栈的栈顶
    fiber->stack_lower = (char *)stack + stack_size - page_size;    // 协程栈目前所能使用内存的最低位地址（可读可写的内存最低地址，初始时为一个页）
    fiber->sched = rt;  // 将调度器关联到协程上
    // 初始化协程
    ef_fiber_init(fiber, fiber_proc, param);
    return fiber;
}

void ef_fiber_init(ef_fiber_t *fiber, ef_fiber_proc_t fiber_proc, void *param)
{
    // 当前协程所对应栈的栈顶指针（即从可读可写的内存中，划分出来一块作为当前协程栈），默认是指向上面划分的可读可写的一个page的内存的128B处
    fiber->stack_ptr = ef_fiber_internal_init(fiber, fiber_proc, (param != NULL) ? param : fiber);
}

void ef_fiber_delete(ef_fiber_t *fiber)
{
    /*
     * free the stack area, contains the ef_fiber_t
     * of course the fiber cannot delete itself
     */
    munmap(fiber->stack_area, fiber->stack_size);
}

// 切换到to协程执行
int ef_fiber_resume(ef_fiber_sched_t *rt, ef_fiber_t *to, long sndval, long *retval)
{
    long ret;
    ef_fiber_t *current;

    /*
     * ensure the fiber is initialized and not exited
     */
    if (to->status != FIBER_STATUS_INITED) {
        if (to->status == FIBER_STATUS_EXITED) {
            return ERROR_FIBER_EXITED;
        }
        return ERROR_FIBER_NOT_INITED;
    }

    // 将to协程作为当前执行协程
    current = rt->current_fiber;
    to->parent = current;
    rt->current_fiber = to;
    // 真正切换逻辑
    // ret == sndval
    ret = ef_fiber_internal_swap(to->stack_ptr, &current->stack_ptr, sndval);

    if (retval) {
        *retval = ret;
    }
    return 0;
}

long ef_fiber_yield(ef_fiber_sched_t *rt, long sndval)
{
    ef_fiber_t *current = rt->current_fiber;
    rt->current_fiber = current->parent;
    return ef_fiber_internal_swap(current->parent->stack_ptr, &current->stack_ptr, sndval);
}

// 扩展当前协程栈空间
// fiber是当前运行的协程，addr是发生SIGSEGV错误的位置
int ef_fiber_expand_stack(ef_fiber_t *fiber, void *addr)
{
    int retval = -1;

    /*
     * align to the nearest lower page boundary
     */
    // 将发生错误的地址，对齐到其所在内存页的首地址
    char *lower = (char *)((long)addr & ~(ef_page_size - 1));

    /*
     * the last one page keep unmaped for guard
     */
    if (lower - (char *)fiber->stack_area >= ef_page_size &&
        lower < (char *)fiber->stack_lower) {
        size_t size = (char *)fiber->stack_lower - lower;
        /**
         * 在Linux中，mprotect()函数可以用来修改一段指定内存区域的保护属性。
            #include <unistd.h>
            #include <sys/mmap.h>
            int mprotect(const void *start, size_t len, int prot);
            mprotect()函数把自start开始的、长度为len的内存区的保护属性修改为prot指定的值。

            prot可以取以下几个值，并且可以用“|”将几个属性合起来使用：
            1）PROT_READ：表示内存段内的内容可写；
            2）PROT_WRITE：表示内存段内的内容可读；
            3）PROT_EXEC：表示内存段中的内容可执行；
            4）PROT_NONE：表示内存段中的内容根本没法访问。
            需要指出的是，指定的内存区间必须包含整个内存页（4K）。区间开始的地址start必须是一个内存页的起始地址，并且区间长度len必须是页大小的整数倍。

            如果执行成功，则返回0；如果执行失败，则返回-1，并且设置errno变量，说明具体因为什么原因造成调用失败。错误的原因主要有以下几个：
            1）EACCES
              该内存不能设置为相应权限。这是可能发生的，比如，如果你 mmap(2) 映射一个文件为只读的，接着使用 mprotect() 标志为 PROT_WRITE。
            2）EINVAL
              start 不是一个有效的指针，指向的不是某个内存页的开头。
            3）ENOMEM
              内核内部的结构体无法分配。
            4）ENOMEM
              进程的地址空间在区间 [start, start+len] 范围内是无效，或者有一个或多个内存页没有映射。
            如果调用进程内存访问行为侵犯了这些设置的保护属性，内核会为该进程产生 SIGSEGV （Segmentation fault，段错误）信号，并且终止该进程。
         * */
        retval = mprotect(lower, size, PROT_READ | PROT_WRITE);
        if (retval >= 0) {
            fiber->stack_lower = lower;
        }
    }
    return retval;
}

// SIGSEGV信号处理函数
void ef_fiber_sigsegv_handler(int sig, siginfo_t *info, void *ucontext)
{
    /*
     * we need core dump if failed to expand fiber stack
     */
    // siginfo_t.si_addr: 错误发生的地址
    if ((SIGSEGV != sig && SIGBUS != sig) ||
        ef_fiber_expand_stack(ef_fiber_sched->current_fiber, info->si_addr) < 0) {
        raise(SIGABRT);
    }
}

// 初始化协程调度器
// 1.将当前系统线程当成当前运行的协程
// 2.注册SIGSEGV信号处理函数：扩展协程的栈空间
int ef_fiber_init_sched(ef_fiber_sched_t *rt, int handle_sigsegv)
{
    stack_t ss;
    struct sigaction sa = {0};

    /*
     * the global pointer used by SIGSEGV handler
     */
    // 协程调度器
    ef_fiber_sched = rt;

    // 将当前系统线程当成当前运行的协程
    rt->current_fiber = &rt->thread_fiber;
    // 设置内存页的大小
    ef_page_size = sysconf(_SC_PAGESIZE);
    if (ef_page_size < 0) {
        return -1;
    }

    if (!handle_sigsegv) {
        return 0;
    }

    /*
     * use alt stack, when SIGSEGV caused by fiber stack, user stack maybe invalid
     * 使用备用栈，当协程栈导致SIGSEGV时，用户栈是无效的
     */
    /**
     * 信号处理函数默认会在进程栈创建一个栈帧，但当进程栈的大小到达了限制值的时候，进程会收到SIGSEGV信号，于是进程便不能创建栈帧了，所以程序就直接执行其默认行为(终止进程) 。
     * 为了解决这个情况，提出了一个备用栈的概念 使得栈帧在这里创建。
        操作如下：
        首先要分配一块内存，可以是静态申请的也可以是动态申请的，这个主要使用结构体 struct stack_t。
        然后使用系统调用 int sigaltstack(const stack_t *sigstack , stack_t * old_sigstack ); //头文件 signal.h
        之后创建信号处理器函数的时候将标志设置成 SA_ONSTACK ，通知内核在备选栈中创建栈帧。

        typedef struct {
            void *ss_sp; // 备选栈的地址
            int ss_flags; // 各种设置掩码
            size_t ss_size; // 备选栈的大小，一般设置为宏SIGSTKSZ
        } stack_t ;
     * */
    ss.ss_sp = mmap(NULL, SIGSTKSZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ss.ss_sp == NULL) {
        return -1;
    }
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1) {
        return -1;
    }

    /*
     * register SIGSEGV handler for fiber stack expanding
     */
    /**
     * int sigaction(int signo, const struct sigaction* act, struct sigaction* oact);
     * sigaction 用来设置或修改指定信号的 action (处理动作)。若参数 oact 非空，则系统会通过其返回 old action。
     *
            # define sa_sigaction __sigaction_handler.sa_sigaction

            struct sigaction
            {
              // Signal handler.
                union
                {
                    __sighandler_t sa_handler;
                    void (*sa_sigaction) (int, siginfo_t *, void *);
                } __sigaction_handler;


                // Additional set of signals to be blocked.
                __sigset_t sa_mask;

                // Special flags.
                int sa_flags;

                // Restore handler.
                void (*sa_restorer) (void);
            };

            参数 sa_mask 指定一个信号集，当信号处理程序被调用时，系统会阻塞这些信号。并且当前信号(参数 signo 指定)会被自动加入到这个信号集，这样保证了在处理指定的信号时，如果该信号再次发生，它会被阻塞，直到前一个信号处理结束。

            参数 sa_flags 可以指定一些选项，如：SA_SIGINFO、SA_ONSTACK、SA_RESTART、SA_RESTORER。
                SA_SIGINFO，则表示使用 _sa_sigaction信号处理程序 (默认是_sa_handler)，通过参数 info 能够得到一些产生信号的信息。比如struct siginfo中有一个成员 si_code，当信号是 SIGBUS 时，如果 si_code 为 BUS_ADRALN，则表示“无效的地址对齐”。
                SA_RESTORER 与 sa_restorer 配对使用，貌似也是为了返回旧的信号处理程序，但现在应该是已经弃用了。
                SA_ONSTACK 表示使用一个备用栈。
            SA_RESTART：使被信号中断的系统调用能够重新发起。
     * */
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sa.sa_sigaction = ef_fiber_sigsegv_handler;
    // 设置 SIGSEGV 信号的处理函数为ef_fiber_sigsegv_handler，同时当中断发生时调用信号处理函数时，使用备用栈
    if (sigaction(SIGSEGV, &sa, NULL) < 0) {
        return -1;
    }

    /*
     * maybe SIGBUS on macos
     */
    if (sigaction(SIGBUS, &sa, NULL) < 0) {
        return -1;
    }
    return 0;
}
