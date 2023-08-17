#include "kernel/types.h"
#include "user/user.h"

void func(int *input, int count1){

    //只剩下一个数待除时，表示input为素数，直接输出并退出函数
	if(1 == count1 ){
		printf("prime %d\n", *input);
		return;
	}

	int pipeTrans[2];//管道
	int prime = *input;//目前已经确定的素数
	int temp;//暂存管道中的数字

	printf("prime %d\n", prime);//输出当前素数
	pipe(pipeTrans);//创建管道

    if(0 == fork())//判断是否为子进程
    {
        //把input数组中的数依次输入管道
        for(int count2 = 0; count2 < count1; count2++){
            temp = *(input + count2);
            //将int强制转换为四字节char，方便输入管道
			write(pipeTrans[1], (char *)(&temp), 4);
		}
        exit(0);
    }

	close(pipeTrans[1]);//立刻关闭管道写入端

	if(0 == fork())//判断是否为子进程
    {

		int count3 = 0;
		char receiced[4];//接收管道中传来的字节

		while(read(pipeTrans[0], receiced, 4) != 0){
            //将管道传来的四字节char强制转换为int并暂存
			temp = *((int *)receiced);
            //temp是潜在素数
			if(temp % prime != 0){
				*input = temp;
				input += 1;
				count3++;
			}
		}
		func(input - count3, count3);
		exit(0);
    }
	wait(0);
	wait(0);
}

//主函数
int main(){
    int input[34];
	
	for(int count4 = 0; count4 < 34; count4++){
		input[count4] = count4+2;
	}
	func(input, 34);
    exit(0);
}