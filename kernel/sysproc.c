#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
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

    argint(0, &n);
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

#ifdef LAB_PGTBL
#define PGACCESS_MAX_PAGE 32
int sys_pgaccess(void)
{
    // 从用户态获取参数
    uint64 va, buf; // 起始虚拟地址和结果缓冲区的用户地址
    int pgnum; // 要检查的页面数量
    argaddr(0, &va); // 获取第一个参数：起始虚拟地址
    argint(1, &pgnum); // 获取第二个参数：页面数量
    argaddr(2, &buf); // 获取第三个参数：结果缓冲区

    // 如果页面数量大于PGACCESS_MAX_PAGE，将其截断为最大值
    if (pgnum > PGACCESS_MAX_PAGE)
        pgnum = PGACCESS_MAX_PAGE;

    // 获取当前进程
    struct proc *p = myproc();
    if (!p) {
        return -1; // 如果进程不存在，返回错误
    }

    // 获取当前进程的页表
    pagetable_t pgtbl = p->pagetable;
    if (!pgtbl) {
        return -1; // 如果页表不存在，返回错误
    }

    // 初始化位掩码为0
    uint64 mask = 0;

    // 遍历要检查的页面数量
    for (int i = 0; i < pgnum; i++) {
        // 获取当前虚拟地址对应的页表项
        pte_t *pte = walk(pgtbl, va + i * PGSIZE, 0);

        // 如果页表项表明页面已被访问（PTE_A标志位）
        if (*pte & PTE_A) {
            *pte &= (~PTE_A); // 复位PTE_A标志位
            mask |= (1 << i); // 标记位掩码中的第i位为1，表示第i个页被访问过
        }
    }

    // 将位掩码拷贝到用户空间的结果缓冲区中
    copyout(p->pagetable, buf, (char *)&mask, sizeof(mask));

    return 0; // 返回成功
}

#endif

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
