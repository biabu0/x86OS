#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H

#include "core/syscall.h"
#include "os_cfg.h"
#include <sys/stat.h>

typedef struct _syscall_args_t{
    int id;//系统调用的id
    int arg0;
    int arg1;
    int arg2;
    int arg3;
}syscall_srgs_t;


void msleep(int ms);
int getpid(void);
//临时用作调试，第一个格式化的字符串，第二个传入的参数为整形
void print_msg(const char * fmt, int arg);
int fork(void);
int execve(const char * pathname, char * const argv[], char * const envp[]);
int yield(void);

int open(const char * name, int flags, ...);
int read(int file, char * ptr, int len);
int write(int file, char * ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);

int isatty(int file);
int fstat(int file, struct stat * st);
void * sbrk (ptrdiff_t incr);

int dup(int file);

#endif