#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_exit(void)
{
    int n;
    argint(0, &n);
    exit(n);
    return 0; // not reached
}

uint64 sys_getpid(void)
{
    return myproc()->pid;
}

uint64 sys_fork(void)
{
    return fork();
}

uint64 sys_wait(void)
{
    uint64 p;
    argaddr(0, &p);
    return wait(p);
}

uint64 sys_sbrk(void)
{
    uint64 addr;
    int n;

    argint(0, &n);
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64 sys_sleep(void)
{
    int n;
    uint ticks0;
    backtrace();
    argint(0, &n);
    if (n < 0)
        n = 0;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (killed(myproc())) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

uint64 sys_kill(void)
{
    int pid;

    argint(0, &pid);
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void)
{
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

// sys_sigalarm - 设置 sigalarm 的间隔和处理函数的系统调用实现
uint64 sys_sigalarm(void)
{
    int interval;        // 闹钟调用的间隔
    uint64 handler;      // 处理函数的地址
    struct proc *p = myproc(); // 获取当前进程的信息

    // 从用户空间获取第一个参数，即间隔，存储在 interval 变量中
    argint(0, &interval);
    // 从用户空间获取第二个参数，即处理函数的地址，存储在 handler 变量中
    argaddr(1, &handler);

    // 将获取到的间隔存储在当前进程的数据结构中，以备后续使用
    p->interval = interval;
    // 将获取到的处理函数地址存储在当前进程的数据结构中，以备后续使用
    p->handler = handler;

    // 返回 0 表示系统调用成功执行
    return 0;
}

// sys_sigreturn - 执行 sigreturn 的系统调用实现
uint64 sys_sigreturn(void)
{
    struct proc *p = myproc(); // 获取当前进程的信息

    if (p->trapframe_saved) {
        // 如果之前保存了陷阱帧（trapframe_saved 非空）
        // 则将陷阱帧恢复为之前保存的状态
        memmove(p->trapframe, p->trapframe_saved, sizeof(*p->trapframe_saved));
        kfree((void *)p->trapframe_saved); // 释放之前保存的陷阱帧内存
        p->trapframe_saved = 0; // 将 trapframe_saved 设为空，表示恢复完成
    }
    p->ticks = 0; // 重置进程的 tick 数为 0，用于 sigalarm 的间隔计数
    return p->trapframe->a0; // 返回陷阱帧中的 a0 寄存器值
}
