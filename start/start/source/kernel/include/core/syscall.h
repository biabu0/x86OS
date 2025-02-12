#ifndef SYSCALL_H
#define SYSCALL_H
#include "comm/types.h"

//进程相关
#define SYS_sleep       0
#define SYS_getpid      1
#define SYS_fork        2
#define SYS_execve      3
#define SYS_yield       4

//文件相关
#define SYS_open        50
#define SYS_read        51
#define SYS_write       52
#define SYS_lseek       53
#define SYS_close       54


#define SYS_fstat       55
#define SYS_sbrk        56
#define SYS_isatty      57
#define SYS_dup         58

#define SYS_printmsg    100

#define SYSCALL_PARAM_COUNT 5

typedef struct _syscall_frame_t{
    int eflags;
    int gs, fs, es, ds;
    uint32_t edi, esi, ebp, dummy, ebx, edx, ecx, eax;
    int eip, cs;
    int  function_id, arg0, arg1, arg2, arg3;
    int esp, ss;
}syscall_frame_t;

//涉及寄存器的操作使用汇编实现
void exception_handler_syscall(void);


#endif