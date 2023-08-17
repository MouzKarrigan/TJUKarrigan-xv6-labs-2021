#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int count2; // 存储读取的字节数
    int count3 = 0, count5 = 0; // 计数器，用于不同目的的计数

    char blk[32], buf[32]; // 缓冲区，用于读取和处理数据
    char *buff = buf; // 指向缓冲区当前位置的指针
    char *lineSpl[32]; // 用于存储拆分后的行的单词

    // 循环通过命令行参数填充 lineSpl 数组
    for (int count1 = 1; count1 < argc; count1++)
    {
        lineSpl[count3++] = argv[count1];
    }

    // 从标准输入读取数据，以 32 字节为一块
    while ((count2 = read(0, blk, sizeof(blk))) > 0)
    {
        for (int count4 = 0; count4 < count2; count4++)
        {
            if (blk[count4] == '\n') // 如果遇到换行符（'\n'）
            {
                buf[count5] = 0; // 在缓冲区末尾添加 null 终止符
                count5 = 0; // 重置缓冲区索引
                lineSpl[count3++] = buff; // 将当前缓冲区存储在 lineSpl 数组中
                buff = buf; // 重置 buff 指针到缓冲区开头
                lineSpl[count3] = 0; // 在 lineSpl 数组末尾添加 null 终止符
                count3 = argc - 1; // 重置 count3 到第一个命令行参数的索引
                
                if (fork() == 0) // 创建子进程
                {
                    exec(argv[1], lineSpl); // 执行指定命令及其参数
                }
                wait(0); // 等待子进程完成
            }
            else if (blk[count4] == ' ') // 如果遇到空格字符
            {
                buf[count5++] = 0; // 在缓冲区末尾添加 null 终止符
                lineSpl[count3++] = buff; // 将当前缓冲区存储在 lineSpl 数组中
                buff = &buf[count5]; // 将 buff 指针移到下一个位置
            }
            else
            {
                buf[count5++] = blk[count4]; // 将字符添加到缓冲区
            }
        }
    }
    exit(0); // 程序正常退出
}
