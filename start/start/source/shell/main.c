#include "lib_syscall.h"
#include <stdio.h>


char cmd_buf[256];

int main (int argc, char **argv){
#if 0
    sbrk(0);
    sbrk(100);
    sbrk(200);
    sbrk(4096*2 + 200);
    sbrk(4096*5 + 1234);

    printf("abef\b\b\b\bcd\n");         //
    printf("abcd\x7f;fdd\n");           //abc;fdd
    printf("\0337Hello, world!\0338123\n");     //123lo, world
    printf("\033[31;42mHello,word!\033[39;49m123\n");       //背景颜色为红色，前景颜色为绿色
    printf("123\033[2DHello,word\n");       //  左移2个字符
    printf("123\033[2CHello,word\n");       //  右移2个字符
    printf("\033[31m");
    printf("\033[10;10H test!\n");//定位到10行10列
    printf("\033[2J");
#endif
    open(argv[0], 0);               //打开tty设备，返回的id是0   stdin
    dup(0);                         //1   stdout        
    //dup 函数用于复制一个现有的文件描述符，返回一个新的文件描述符，这个新的文件描述符与原文件描述符指向同一个文件、管道或设备。
    dup(0);               //2   stderr


    printf("Hello from shell\n");
    printf("OS version : %s\n", "1.0.0");




    // for(int i = 0; i < argc; i++){
    //     printf("arg: %s\n", argv[i]);
    // }

    // fork();
    // yield();

    for(;;){
        gets(cmd_buf);//从标准输入里面读取一个字符串->会调用sys_read读取
        puts(cmd_buf);//输出到标准输出->会调用sys_write写入
        // printf("shell pid = %d\n", getpid());
        // msleep(1000);
    };
}