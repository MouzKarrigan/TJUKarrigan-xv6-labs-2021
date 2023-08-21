// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13 // 哈希桶上限，按理来说只有需要二次哈希时才考虑素数个桶
#define INTMAX 0x7fffffff

// 哈希函数
int hash(uint dev, uint blockno)
{
    return ((blockno) % NBUCKET);
}

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // 维护NBUCKET个桶，每个桶维护一个链表
    // 每个桶都有一个自己的锁，用于保护自己的链表
    // 而每个桶中存储的元素buf作为缓冲区存在自己的锁
    struct buf bucket[NBUCKET];
    struct spinlock bucket_locks[NBUCKET];
} bcache;

void binit(void)
{
    // 初始化整个块缓存结构
    initlock(&bcache.lock, "bcache_lock");

    // 初始化每个bucket的锁和链表头
    char name[32];
    for (int i = 0; i < NBUCKET; i++) {
        snprintf(name, 32, "bucket_lock_%d", i);
        initlock(&bcache.bucket_locks[i], name);
        bcache.bucket[i].next = 0; // 链表头指针初始化为NULL
    }

    // 初始化每个buffer
    for (int i = 0; i < NBUF; i++) {
        struct buf *b = &bcache.buf[i]; // 获取当前buffer的指针
        initsleeplock(&b->lock, "buffer"); // 初始化buffer的睡眠锁，用于保护对buffer的并发访问

        b->LUtime = 0; // 上次被访问的时间戳，初始为0
        b->refcnt = 0; // 引用计数，初始为0，表示当前没有进程在使用该buffer
        b->curBucket = 0; // 当前所在的bucket索引，初始为0

        // 将buffer加入到bucket[0]中，即初始化的时候将所有buffer放入第一个bucket中
        b->next = bcache.bucket[0].next;
        bcache.bucket[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno)
{
    uint index = hash(dev, blockno); // 计算哈希索引

    acquire(&bcache.bucket_locks[index]); // 获取当前bucket的锁
    struct buf *b = bcache.bucket[index].next; // 获取当前bucket的第一个块

    // 在当前bucket中查找指定的块
    while (b) {
        if (b->dev == dev && b->blockno == blockno) {
            // 如果找到了指定块
            b->refcnt++; // 块的引用计数加一
            release(&bcache.bucket_locks[index]); // 释放当前bucket的锁
            acquiresleep(&b->lock); // 获取块的sleep锁，确保不会被其他线程并发访问
            return b; // 返回找到的块
        }
        b = b->next; // 继续遍历bucket中的块
    }

    // 在当前bucket中未找到指定块，需要检查其他bucket
    // 在检查其他bucket时，不可以同时持有多个bucket的锁，需要先释放当前bucket的锁
    release(&bcache.bucket_locks[index]);

    acquire(&bcache.lock); // 获取整个bcache的锁
    b = bcache.bucket[index].next; // 获取当前bucket的第一个块

    while (b) {
        if (b->dev == dev && b->blockno == blockno) {
            // 如果找到了指定块，但在其他bucket中
            acquire(&bcache.bucket_locks[index]); // 获取当前bucket的锁
            b->refcnt++; // 块的引用计数加一
            release(&bcache.bucket_locks[index]); // 释放当前bucket的锁
            release(&bcache.lock); // 释放整个bcache的锁

            acquiresleep(&b->lock); // 获取块的sleep锁，确保不会被其他线程并发访问
            return b;
        }
        b = b->next; // 继续遍历bucket中的块
    }

    // 如果在其他bucket中也未找到，需要根据LRU策略查找一个最适合替换的块
    struct buf *LRUb = 0; // 用于记录最适合替换的块
    uint curBucket = -1; // 当前的bucket索引
    uint LUtime = INTMAX; // 最早的使用时间

    for (int i = 0; i < NBUCKET; i++) {
        acquire(&bcache.bucket_locks[i]); // 获取当前bucket的锁
        b = &bcache.bucket[i]; // 获取当前bucket的链表头
        int found = 0; // 标志是否找到合适的块

        while (b->next) {
            if (b->next->refcnt == 0 && (LRUb == 0 || b->next->LUtime < LUtime)) {
                // 找到一个空闲的块，或者找到更早未使用的块
                LRUb = b; // 更新LRUb
                LUtime = b->next->LUtime; // 更新最早使用时间
                found = 1;
            }
            b = b->next; // 继续遍历bucket中的块
        }

        if (found) {
            if (curBucket != -1) {
                release(&bcache.bucket_locks[curBucket]); // 释放之前的bucket锁
            }
            curBucket = i; // 更新当前bucket索引
        } else {
            release(&bcache.bucket_locks[i]); // 释放当前bucket的锁
        }
    }

    if (LRUb == 0) {
        panic("bget: No buffer."); // 所有块都在使用中，无法分配新的块
    } else {
        struct buf *p = LRUb->next; // 获取要替换的块

        if (curBucket != index) {
            LRUb->next = p->next; // 从其他bucket中删除p
            release(&bcache.bucket_locks[curBucket]); // 释放之前的bucket锁

            acquire(&bcache.bucket_locks[index]); // 获取当前bucket的锁
            p->next = bcache.bucket[index].next; // 将p插入当前bucket的链表头
            bcache.bucket[index].next = p;
        }

        p->dev = dev;
        p->blockno = blockno;
        p->refcnt = 1;
        p->valid = 0;
        p->curBucket = index;

        release(&bcache.bucket_locks[index]); // 释放当前bucket的锁
        release(&bcache.lock); // 释放整个bcache的锁
        acquiresleep(&p->lock); // 获取p块的sleep锁
        return p; // 返回要分配的块
    }
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    uint index = hash(b->dev, b->blockno);
    acquire(&bcache.bucket_locks[index]);
    b->refcnt--;
    if (b->refcnt == 0) {
        //没有进程引用这块buffer, 则为空闲状态释放，记录最近使用时间
        b->LUtime = ticks;
    }
    release(&bcache.bucket_locks[index]);
}

void bpin(struct buf *b)
{

    uint index = hash(b->dev, b->blockno);
    acquire(&bcache.bucket_locks[index]);
    b->refcnt++;
    release(&bcache.bucket_locks[index]);
}

void bunpin(struct buf *b)
{

    uint index = hash(b->dev, b->blockno);
    acquire(&bcache.bucket_locks[index]);
    b->refcnt--;
    release(&bcache.bucket_locks[index]);
}