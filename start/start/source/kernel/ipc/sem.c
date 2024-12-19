#include "ipc/sem.h"
#include "core/task.h"
#include "cpu/irq.h"

void sem_init(sem_t * sem, int init_count){
    sem->count = init_count;
    list_init(&sem->wait_list);
}

void sem_wait(sem_t *sem){
    //可能有多个进程同时操作，所以需要上锁
    irq_state_t state = irq_enter_protection();
    if(sem->count > 0){
        sem->count--;    //当前进程获得一个信号，进程继续运行，无需要等待
    }else{      //信号量为0，进程需要等待，加入到信号量的等待队列中
        task_t * curr = task_current();
        task_set_block(curr);   //从就绪队列中移除
        list_insert_last(&sem->wait_list, &curr->wait_node);
        task_dispatch();    //当前正在运行的进程从就绪队列中移除，要进行任务切换
    }
    irq_leave_protection(state);
}
void sem_notify (sem_t * sem){
    irq_state_t state = irq_enter_protection();
    if(list_count(&sem->wait_list)){
        list_node_t * node = list_remove_first(&sem->wait_list);
        task_t * task = list_node_parent(node, task_t, wait_node);
        task_set_ready(task);
        task_dispatch();
    }else{
        sem->count++;
    }
    irq_leave_protection(state);
}

int sem_count (sem_t * sem){
    irq_state_t state = irq_enter_protection();
    int count = sem->count;
    irq_leave_protection(state);

    return count;
}
