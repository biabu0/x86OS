#ifndef TTY_H
#define TTY_H

#include "ipc/sem.h"
#define TTY_OBUF_SIZE   512
#define TTY_IBUF_SIZE   512
#define TTY_NR          8

#define TTY_OCRLF        (1 << 0)//'\n'的功能选择的宏
#define TTY_INCLR     (1 << 0)   
#define TTY_IECHO        (1 << 1)       //输入的回显

//定义了一个先入先出（FIFO）缓冲区，用于存储字符流。
typedef struct _tty_fifo_t{
    char * buf;                 //指向缓存区
    int size;                   //大小
    int read, write;            //读写指针
    int count;                  //当前缓存区中有效数据
}tty_fifo_t;



//TTY设备的基本属性，包括输入输出缓冲区
typedef struct _tty_t{
    char obuf[TTY_OBUF_SIZE];
    tty_fifo_t ofifo;
    sem_t osem;
    char ibuf[TTY_IBUF_SIZE];
    tty_fifo_t ififo;
    sem_t isem;                     //中断处理程序中向进程发送通知

    int iflags;
    int oflags;

    //哪一块显存中写数据
    int console_idx;
}tty_t;

int tty_fifo_put(tty_fifo_t * fifo, char c);
int tty_fifo_get(tty_fifo_t * fifo, char * c);
void tty_select(int tty);
void tty_in(char ch);
#endif