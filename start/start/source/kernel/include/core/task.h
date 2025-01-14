#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "comm/types.h"
#include "tools/list.h"

#define TASK_TIME_SLICE_DEFAULT 10  //10次定时中断
#define TASK_NAME_SIZE 32

#define TASK_FLAGS_SYSTEM (1 << 0)


typedef struct _task_args_t{
    uint32_t ret_addr;
    uint32_t argc;
    char ** argv;
}task_args_t;

//会使用task_t描述进程（一个程序的运行）
typedef struct _task_t{
    //uint32_t * stack; //stack top pointer, esp

    enum {
        TASK_CREATED,
        TASK_RUNNING,
        TASK_SLEEP, //延时状态
        TASK_READY,
        TASK_WAITTING,
    }state;
    pid_t pid;
    struct _task_t * parent;
    
    int sleep_ticks;        //延时计数器，每次10ms（定时器中断的值）
    int time_ticks;     //计数器
    int slice_ticks;    //这里设置为10，递减的，定时器中断是10ms中断一次，所以,一个进程最多运行时间是100ms

    char name[TASK_NAME_SIZE];

    list_node_t wait_node;  //用于插入信号量等待队列中
    list_node_t run_node;   //用于插入就绪队列中ready_list
    list_node_t all_node;   //用于插入进程队列中task_list

    tss_t tss;
    int tss_sel;
}task_t;

// 传入程序的入口地址， 栈的指针
int task_init(task_t * task, const char * name, int flag, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);

void task_time_tick(void);


typedef struct _task_manager_t{
    task_t * cur_task;      //当前正在运行的进程

    list_t ready_list;
    list_t task_list;
    list_t sleep_list;

    task_t first_task;  //init_main进程的task_t类型定义到这里
    task_t idle_task;   //空闲进程

    int app_code_sel;
    int app_data_sel;
}task_manager_t;
// 初始化进程管理器
void task_mananger_init(void);
// 初始化第一个进程，也就是任务管理器中的first_task
void task_first_init(void);
// 获取任务管理器中的第一个进程
task_t * task_first_task(void);

void task_set_ready(task_t * task);
void task_set_block(task_t* task);

int sys_sched_yield(void);

void task_dispatch(void);
task_t * task_current(void);

// 在睡眠队列中待多少个时钟节拍
void task_set_sleep(task_t * task, uint32_t ticks);
// 从睡眠队列中移除
void task_set_wakeup(task_t * task);

void sys_sleep(uint32_t ms);
int sys_getpid(void);
int sys_fork(void);
int sys_execve(char * pathname, char * argv[], char * envp[]);

#endif