#ifndef MUTEX_H
#define MUTEX_H

#include "core/task.h"
#include "tools/list.h"

typedef struct _mutex_t {
    task_t * owner;
    int locker;         //上锁多少次，出现嵌套上锁的过程，记录上锁次数
    list_t wait_list;   // 等待队列，放置等待互斥锁的进程
}mutex_t;

void mutex_init(mutex_t * mutex);

//上锁
void mutex_locK(mutex_t * mutex);
//解锁
void mutex_unlock(mutex_t * mutex);


#endif