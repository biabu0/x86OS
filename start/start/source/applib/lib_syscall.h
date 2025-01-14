#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H

#include "core/syscall.h"
#include "os_cfg.h"

typedef struct _syscall_args_t{
    int id;//系统调用的id
    int arg0;
    int arg1;
    int arg2;
    int arg3;
}syscall_srgs_t;


static inline int sys_call(syscall_srgs_t *args){
    //CALL指令：： 0:偏移量，调用门选择子的偏移量，不需要设置为0，SELECTOR_SYSCALL为调用门描述符在GDT中的索引，0：RPL的值
    uint32_t addr[] = {0, SELECTOR_SYSCALL | 0};
    int ret;
    __asm__ __volatile__(
        "push %[arg3]\n\t"
        "push %[arg2]\n\t"
        "push %[arg1]\n\t"
        "push %[arg0]\n\t"
        "push %[id]\n\t"
	    "lcalll *(%[a])"
        :"=a"(ret)
        :[arg3]"r"(args->arg3),
        [arg2]"r"(args->arg2),
        [arg1]"r"(args->arg1),
        [arg0]"r"(args->arg0),
        [id]"r"(args->id),
        [a]"r"(addr));
    //将eax中的值取出来，返回
    return ret;
}
static inline void msleep(int ms){
    if(ms <= 0){
        return ;
    }
    syscall_srgs_t args;
    args.id = SYS_sleep;
    args.arg0 = ms;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);
}

static inline int getpid(void){
    syscall_srgs_t args;
    args.id = SYS_getpid;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
//临时用作调试，第一个格式化的字符串，第二个传入的参数为整形
static inline void print_msg(const char * fmt, int arg){
    syscall_srgs_t args;
    args.id = SYS_printmsg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);
}
static inline int fork(void){
    syscall_srgs_t args;
    args.id = SYS_fork;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);
}


static inline int execve(const char * pathname, char * const argv[], char * const envp[]){
    syscall_srgs_t args;
    args.id = SYS_execve;
    args.arg0 = (int)pathname;
    args.arg1 = (int)argv;
    args.arg2 = (int)envp;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}


static inline int yield(void){
    syscall_srgs_t args;
    args.id = SYS_yield;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);

}

#endif