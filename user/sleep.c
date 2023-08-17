#include "kernel/types.h"
#include "user/user.h"

int main(int arg,char *argv[])
{
    if(2!=arg)
    {
        printf("Arg amount error\n");
        exit(1);
    }
    else
    {
        int sleepNumber=atoi(argv[1]);
        sleep(sleepNumber);
    }
    exit(0);
}