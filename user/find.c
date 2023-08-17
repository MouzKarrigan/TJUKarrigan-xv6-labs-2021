#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define MAX_PATH_LENGTH 512

// 格式化文件名，去除路径并进行空白填充
char *fmtname(char *path) {
    static char buf[DIRSIZ + 1];
    char *p;

    // 查找最后一个斜杠后的第一个字符
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // 返回空白填充后的文件名
    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    for (char *i = buf + strlen(buf);; i--) {
        if (*i != '\0' && *i != ' ' && *i != '\n' && *i != '\r' && *i != '\t') {
            *(i + 1) = '\0';
            break;
        }
    }
    return buf;
}

// 在指定路径下查找文件，并递归地在子目录中查找
void find(char *path, char *filename) {
    char buf[MAX_PATH_LENGTH];
    char *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 打开指定路径的文件
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取文件状态信息
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 根据文件类型进行处理
    switch (st.type) {
    case T_FILE:
        // 如果文件名与目标文件名相同，输出文件路径
        if (strcmp(fmtname(path), filename) == 0) {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        // 拼接路径并遍历目录中的文件和子目录
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            fprintf(2, "find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                continue;
            }
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            // 获取子文件或子目录的状态信息，并递归调用 find 函数
            if (stat(buf, &st) < 0) {
                fprintf(2, "find: cannot stat %s\n", buf);
                continue;
            }
            find(buf, filename);
        }
        break;
    }

    // 关闭文件
    close(fd);
}

int main(int argc, char *argv[]) {
    // 检查命令行参数数量
    if (argc != 3) {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(0);
    }

    // 调用 find 函数，传递要查找的目录路径和目标文件名
    find(argv[1], argv[2]);
    exit(0);
}





