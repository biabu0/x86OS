#include "ipc/mutex.h"
#include "cpu/irq.h"
#include "core/task.h"
#include "tools/list.h"

void mutex_init(mutex_t * mutex){
    mutex->locker = 0;
    mutex->owner = (task_t *)0;
    list_init(&mutex->wait_list);
}

//上锁
void mutex_locK(mutex_t * mutex){
    irq_state_t state = irq_enter_protection();

    task_t * curr = task_current();
    if(mutex->locker == 0){ //没有上锁
        mutex->locker++;
        mutex->owner = curr;
    }else if(mutex->owner == curr){ //已经上锁，如果是自己，则直接加一
        mutex->locker++;
    }else{          //如果上锁的进程不是自己，则阻塞自己，并加入等待队列
        task_set_block(curr);
        list_insert_last(&mutex->wait_list, &curr->wait_node);
        task_dispatch();//当前任务已经从就绪队列中移除，切换到下一个队列
    }
    irq_leave_protection(state);
}
//解锁
void mutex_unlock(mutex_t * mutex){
    irq_state_t state = irq_enter_protection();
    task_t * curr = task_current();
    if(mutex->owner == curr){
        mutex->locker--;
        if(mutex->locker == 0){     //支持一个进程多次上锁
            mutex->owner = (task_t *)0;     //解锁
            if(list_count(&mutex->wait_list)){      //互斥锁等待队列有进程，则将锁给到该进程

                list_node_t * node = list_remove_first(&mutex->wait_list);
                task_t * task = list_node_parent(node, task_t, wait_node);
                task_set_ready(task);

                mutex->locker++;
                mutex->owner = task;

                task_dispatch();
            }
        }
    }
    
    irq_leave_protection(state);
}
