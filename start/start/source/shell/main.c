#include "lib_syscall.h"
#include <stdio.h>
int main (int argc, char **argv){
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

    printf("Hello from shell\n");
    printf("OS version : %s\n", "1.0.0");
    for(int i = 0; i < argc; i++){
        printf("arg: %s\n", argv[i]);
    }

    fork();
    yield();

    for(;;){
        printf("shell pid = %d\n", getpid());
        msleep(1000);
    };
}