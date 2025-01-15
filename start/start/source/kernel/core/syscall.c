#include "core/syscall.h"
#include "core/task.h"
#include "comm/types.h"
#include "tools/log.h"
#include "fs/fs.h"
#include "core/memory.h"
typedef int (*syscall_handler_t)(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

void sys_print_msg(char * fmt, int arg){
    log_printf(fmt, arg);
}

static const syscall_handler_t sys_table[] = {
    [SYS_sleep] = (syscall_handler_t)sys_sleep,
    [SYS_getpid] = (syscall_handler_t)sys_getpid,
    [SYS_printmsg] = (syscall_handler_t)sys_print_msg,
    [SYS_fork] = (syscall_handler_t)sys_fork,
    [SYS_execve] = (syscall_handler_t)sys_execve,
    [SYS_yield] = (syscall_handler_t)sys_sched_yield,

    [SYS_open] = (syscall_handler_t)sys_open,
    [SYS_close] = (syscall_handler_t)sys_close,
    [SYS_read] = (syscall_handler_t)sys_read,
    [SYS_write] = (syscall_handler_t)sys_write,
    [SYS_lseek] = (syscall_handler_t)sys_lseek,

    [SYS_sbrk] = (syscall_handler_t)sys_sbrk,
    [SYS_fstat] = (syscall_handler_t)sys_fstat,
    [SYS_isatty] = (syscall_handler_t)sys_isatty,
};
void do_handler_syscall(syscall_frame_t * frame){
    if(frame->function_id < sizeof(sys_table)/sizeof(sys_table[0])){
        syscall_handler_t handler = sys_table[frame->function_id];
        if(handler){
            //在sys_sleep中虽然传递四个参数，只是说压入栈中四个值，但函数只是使用了一个值；
            int ret = handler(frame->arg0, frame->arg1, frame->arg2, frame->arg3);
            //eax中存放的是被调用函数的返回值
            frame->eax = ret;
            return ;
        }
    }
    task_t * task = task_current();
    log_printf("task : %s, Unknown syscall: %d", task->name, frame->function_id);
    //没有对用的处理函数，发生错误，设置为-1
    frame->eax = -1;
}