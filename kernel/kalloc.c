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
} kmem;

struct refcnt {
    struct spinlock lock;
    int PGCount[PHYSTOP / PGSIZE]; // (最大页数 = 地址空间大小/页大小)
} PGRefCount;

void kinit()
{
    initlock(&kmem.lock, "kmem");
    initlock(&PGRefCount.lock, "PGRefCount"); // 初始化PGRefCount锁
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
        // 对于所有页的count值设为1; 后续在kfree中减为0, 可以正常进行释放
        PGRefCount.PGCount[(uint64)p / PGSIZE] = 1;
        kfree(p);
    }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
    struct run *r;

    // 检查页面地址的合法性
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // 获取物理页面引用计数的锁
    acquire(&PGRefCount.lock);

    // 减少物理页面的引用计数
    PGRefCount.PGCount[(uint64)pa / PGSIZE]--;

    // 如果引用计数不为零，则释放锁并返回
    if (PGRefCount.PGCount[(uint64)pa / PGSIZE] != 0) {
        release(&PGRefCount.lock);
        return;
    }

    // 引用计数为零，释放引用计数锁
    release(&PGRefCount.lock);

    // 用填充数据初始化页面，以防止悬挂引用
    memset(pa, 1, PGSIZE);

    // 将释放的页面添加到空闲链表中
    r = (struct run *)pa;

    // 获取内核内存分配锁
    acquire(&kmem.lock);

    // 将释放的页面添加到空闲链表的头部
    r->next = kmem.freelist;
    kmem.freelist = r;

    // 释放内核内存分配锁
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void)
{
    struct run *r;

    // 获取内核内存分配锁
    acquire(&kmem.lock);
    
    // 从空闲链表中获取一个空闲页
    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;
    
    // 释放内核内存分配锁
    release(&kmem.lock);

    // 如果成功获取了空闲页
    if (r) {
        // 获取物理页面引用计数的锁
        acquire(&PGRefCount.lock);
        
        // 设置物理页面的引用计数为1
        PGRefCount.PGCount[(uint64)r / PGSIZE] = 1;
        
        // 释放物理页面引用计数的锁
        release(&PGRefCount.lock);
    }

    // 如果成功获取了空闲页
    if (r)
        memset((char *)r, 5, PGSIZE); // 使用填充数据初始化页面

    // 返回获取到的内核页面的指针
    return (void *)r;
}

int AddPGRefCount(void *pa)
{
    // 检查物理地址是否是页面大小的倍数
    if (((uint64)pa % PGSIZE)) {
        return -1;
    }
    
    // 检查物理地址是否在合法范围内
    if ((char *)pa < end || (uint64)pa >= PHYSTOP) {
        return -1;
    }
    
    // 获取物理页面引用计数的锁
    acquire(&PGRefCount.lock);
    
    // 增加对应物理页面的引用计数
    PGRefCount.PGCount[(uint64)pa / PGSIZE]++;
    
    // 释放物理页面引用计数的锁
    release(&PGRefCount.lock);
    
    return 0;
}

int GetPGRefCount(void *pa)
{
    return PGRefCount.PGCount[(uint64)pa / PGSIZE];
}
