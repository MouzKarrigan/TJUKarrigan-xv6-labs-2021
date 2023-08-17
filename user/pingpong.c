#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{

    if (argc > 1) {
        fprintf(2, "No 1 argument is needed!\n");
        exit(1);
    }

    int ptc[2], ctp[2];
    char buf[10];

    if (pipe(ptc) < 0) {
        printf("pipe create failed\n");
        exit(1);
    }

    if (pipe(ctp) < 0) {
        printf("pipe create failed\n");
        exit(1);
    }

    int pid;
    pid = fork();

    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // 子进程
        close(ctp[0]);
        close(ptc[1]);
        // 从管道读取字节
        if (read(ptc[0], buf, 4) == -1) {
            printf("child process read failed\n");
            exit(1);
        }
        close(ptc[0]);

        printf("%d: received %s\n", getpid(), buf);

        strcpy(buf, "pong");
        // 向管道发送字节
        if (write(ctp[1], buf, 4) == -1) {
            printf("child process write failed\n");
            exit(1);
        }
        close(ctp[1]);
        exit(0);
    }
    else {
        strcpy(buf, "ping");
        // 父进程
        close(ctp[1]);
        close(ptc[0]);
        // 向管道发送字节
        if (write(ptc[1], buf, 4) == -1) {
            printf("parent process write failed\n");
            exit(1);
        }

        // 等待子进程结束
        wait(0);

        close(ptc[1]);
        // 从管道读取字节
        if (read(ctp[0], buf, 4) == -1) {
            printf("parent process read failed\n");
            exit(1);
        }

        close(ctp[0]);
        printf("%d: received %s\n", getpid(), buf);

        exit(0);
    }
}