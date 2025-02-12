#include "lib_syscall.h"
 

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
void msleep(int ms){
    if(ms <= 0){
        return ;
    }
    syscall_srgs_t args;
    args.id = SYS_sleep;
    args.arg0 = ms;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);
}

int getpid(void){
    syscall_srgs_t args;
    args.id = SYS_getpid;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
//临时用作调试，第一个格式化的字符串，第二个传入的参数为整形
void print_msg(const char * fmt, int arg){
    syscall_srgs_t args;
    args.id = SYS_printmsg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);
}
int fork(void){
    syscall_srgs_t args;
    args.id = SYS_fork;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);
}


int execve(const char * pathname, char * const argv[], char * const envp[]){
    syscall_srgs_t args;
    args.id = SYS_execve;
    args.arg0 = (int)pathname;
    args.arg1 = (int)argv;
    args.arg2 = (int)envp;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}


int yield(void){
    syscall_srgs_t args;
    args.id = SYS_yield;
    //使用调用门的设置，来调用操作系统内部的参数
    sys_call(&args);

}


int open(const char * name, int flags, ...){
    syscall_srgs_t args;
    args.id = SYS_open;
    args.arg0 = (int)name;
    args.arg1 = (int)flags;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
int read(int file, char * ptr, int len){
    syscall_srgs_t args;
    args.id = SYS_read;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = (int)len;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
int write(int file, char * ptr, int len){
    syscall_srgs_t args;
    args.id = SYS_write;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = (int)len;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);

}
int close(int file){
    syscall_srgs_t args;
    args.id = SYS_close;
    args.arg0 = (int)file;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);

}
int lseek(int file, int ptr, int dir){
    syscall_srgs_t args;
    args.id = SYS_lseek;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = (int)dir;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}

int isatty(int file){
    syscall_srgs_t args;
    args.id = SYS_isatty;
    args.arg0 = (int)file;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
int fstat(int file, struct stat * st){
    syscall_srgs_t args;
    args.id = SYS_fstat;
    args.arg0 = (int)file;
    args.arg1 = (int)st;

    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
void * sbrk (ptrdiff_t incr){
    syscall_srgs_t args;
    args.id = SYS_sbrk;
    args.arg0 = (int)incr;
    //使用调用门的设置，来调用操作系统内部的参数
    return (void *)sys_call(&args);

}

int dup(int file){
    syscall_srgs_t args;
    args.id = SYS_dup;
    args.arg0 = (int)file;
    //使用调用门的设置，来调用操作系统内部的参数
    return sys_call(&args);
}
