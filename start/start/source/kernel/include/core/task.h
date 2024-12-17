#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "comm/types.h"
#include "tools/list.h"


#define TASK_NAME_SIZE 32
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

    char name[TASK_NAME_SIZE];

    list_node_t run_node;   //用于插入就绪队列中ready_list
    list_node_t all_node;   //用于插入进程队列中task_list

    tss_t tss;
    int tss_sel;
}task_t;

// 传入程序的入口地址， 栈的指针
int task_init(task_t * task, const char * name, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);

typedef struct _task_manager_t{
    task_t * cur_task;      //当前正在运行的进程

    list_t ready_list;
    list_t task_list;

    task_t first_task;  //init_main进程的task_t类型定义到这里
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
#endif