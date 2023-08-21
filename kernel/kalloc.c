// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem[NCPU]; // 每个CPU都有维护独有的kmem链表

void kinit()
{
    // 初始化每个CPU的自由列表
    char kmem_name[32];
    for (int i = 0; i < NCPU; i++) {
        snprintf(kmem_name, 32, "kmem_%d", i); // 创建每个CPU对应的内存锁的名称
        initlock(&kmem[i].lock, kmem_name); // 初始化每个CPU对应的内存锁
    }
    freerange(end, (void *)PHYSTOP); // 设置内存可分配范围
}

void freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
    struct run *r;

    // 检查是否满足释放条件，否则引发panic
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // 用垃圾填充以捕获悬空引用。
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    // 当CPU获取任何锁时，xv6总是禁用该CPU上的中断。
    push_off();

    int CPUID = cpuid();
    acquire(&kmem[CPUID].lock);
    // 从链表头插入空闲页
    r->next = kmem[CPUID].freelist;
    kmem[CPUID].freelist = r;
    release(&kmem[CPUID].lock);

    // 当CPU未持有自旋锁时，xv6重新启用中断
    pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void)
{
    struct run *r;

    push_off(); // 关闭中断

    // 获取当前CPU的ID
    int CPUID = cpuid();
    acquire(&kmem[CPUID].lock); // 获取当前CPU的kmem锁

    r = kmem[CPUID].freelist; // 从当前CPU的空闲列表获取一个页

    // 在当前CPU上查找空闲页
    if (r)
        kmem[CPUID].freelist = r->next; // 移除已分配的空闲页

    if (r == 0) { // 若当前CPU上没有空闲页
        // 在其他CPU上查找空闲页
        for (int i = 0; i < NCPU; i++) {
            if (i == CPUID)
                continue; // 跳过当前CPU
            // 要获取其他CPU的锁
            acquire(&kmem[i].lock); // 获取其他CPU的kmem锁
            r = kmem[i].freelist; // 从其他CPU的空闲列表获取一个页
            if (r)
                kmem[i].freelist = r->next; // 移除已分配的空闲页
            release(&kmem[i].lock); // 释放其他CPU的kmem锁
            if (r) // 若找到空闲页，脱离循环
                break;
        }
    }

    release(&kmem[CPUID].lock); // 释放当前CPU的kmem锁
    pop_off(); // 恢复中断

    if (r)
        memset((char *)r, 5, PGSIZE); // 用垃圾填充，以捕捉悬空引用
    return (void *)r; // 返回分配的物理页
}
