#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "comm/types.h"

//会使用task_t描述进程（一个程序的运行）
typedef struct _task_t{
    //uint32_t * stack; //stack top pointer, esp
    tss_t tss;
    int tss_sel;
}task_t;

// 传入程序的入口地址， 栈的指针
int task_init(task_t * task, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);
#endif