#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"

static uint32_t idle_task_stack[IDLE_TASK_STACK_SIZE];
// 整个系统中只需要一个任务管理器，定义为全局变量
static task_manager_t task_manager;
static int tss_init(task_t * task, uint32_t entry, uint32_t esp){
    int tss_sel = gdt_alloc_desc();
    if(tss_sel < 0){
        log_printf("alloc tss failed!\r\n");
        return -1;
    }
    //初始化GDT表项，segment_desc_set函数已经设置好将GDT表中的位置加载进去，也就是说现在GDT表中已经有了这个TSS描述符
    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t), 
        SEG_TYPE_TSS | SEG_DPL0 | SEG_P_PRESENT
    );

    kernel_memset(&task->tss, 0, sizeof(tss_t));    //清空tss
    task->tss.eip = entry;      //CPU指令的位置，第一次运行时，将函数地址给eip保存起来
    task->tss.esp = task->tss.esp0 = esp;       //程序栈顶指针，esp0跟特权级有关，运行在特权级0
    task->tss.ss = task->tss.ss0 = KERNEL_SELECTOR_DS;      //平坦模型，数据段，也会用作栈空间
    task->tss.es = task->tss.ds = task->tss.fs = task->tss.gs = KERNEL_SELECTOR_DS;
    task->tss.cs = KERNEL_SELECTOR_CS;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;      //IF为1，我们不希望从TSS恢复后所有的中断不能响应
    //其他通用寄存器，设置为0，里面的值由程序自己设置
    //cr3，后序课程使用
    uint32_t page_dir = memory_create_uvm();
    if(page_dir == 0){
        //页表创建失败，释放描述符
        gdt_free_sel(tss_sel);
        return -1;
    }
    task->tss.cr3 = page_dir;//将页表设置到cr3
    task->tss_sel = tss_sel;        //将该任务的选择子保存起来，切换任务时，需要用到
    return 0;
}


int task_init(task_t * task, const char * name, uint32_t entry, uint32_t esp){
    ASSERT(task != (task_t *)0);        //初始化，不能0
    tss_init(task, entry, esp); //初始化了tss和tss_sel

    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->sleep_ticks = 0;
    task->time_ticks = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_ticks;

    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);

    // uint32_t * pesp = (uint32_t *)esp;
    // if(pesp){
    //     // first run to_task, need init stack, else error when pop stack
    //     *(--pesp) = entry;
    //     *(--pesp) = 0;
    //     *(--pesp) = 0;
    //     *(--pesp) = 0;
    //     *(--pesp) = 0;
    //     task->stack = (uint32_t *)pesp;
    // }

    return 0;
}

void simple_switch(uint32_t **from, uint32_t *to);
void task_switch_from_to (task_t *from, task_t *to){
    switch_to_tss(to->tss_sel);
    //simple_switch(&from->stack, to->stack);
};


// 初始化第一个进程，也就是任务管理器中的first_task
void task_first_init(void){
    task_init(&task_manager.first_task, "first task", 0, 0);
    write_tr(task_manager.first_task.tss_sel);
    task_manager.cur_task = &task_manager.first_task;

}
// 获取任务管理器中的第一个进程
task_t * task_first_task(void){
    return &task_manager.first_task;
}

static void idle_task_entry(void){
    for(;;){
        hlt();  //调用低功耗指令，让cpu处于低功耗状态，系统的开销就会少很多
    }
}
void task_mananger_init(void){
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    task_manager.cur_task = (task_t *)0;

    task_init(&task_manager.idle_task, 
        "idle task", 
        (uint32_t)idle_task_entry, 
        (uint32_t)idle_task_stack + IDLE_TASK_STACK_SIZE
    );
}

void task_set_ready(task_t *task){
    // 空闲进程不加入就绪队列中
    if(task == &task_manager.idle_task){
        return ;
    }

    list_insert_last(&task_manager.ready_list, &task->run_node);
    task->state = TASK_READY;
}

void task_set_block(task_t * task){
    if(task == &task_manager.idle_task){
        return ;
    }
    list_remove(&task_manager.ready_list, &task->run_node);
    //删除后不确定状态值，也就不需要设置
}


//处于整个队列头部的任务
task_t * task_next_run(void){
    if(list_count(&task_manager.ready_list) == 0){
        return &task_manager.idle_task;
    }
    list_node_t * node = list_first(&task_manager.ready_list);//获取就绪队列的第一个元素
    return list_node_parent(node, task_t, run_node);
}
task_t * task_current(void){
    return task_manager.cur_task;
}

int sys_sched_yield(void){
    //就绪队列中不止一个进程，将当前进程放到就绪队列的最后面
    irq_state_t state = irq_enter_protection();
    if(list_count(&task_manager.ready_list)>1){
        task_t * curr_task = task_current();    //取出当前任务
        task_set_block(curr_task);      //将当前进程从就绪队列中移除
        task_set_ready(curr_task);      //再放到就绪队列的最后面
        
        task_dispatch();
    }
    irq_leave_protection(state);
    return 0;
}


// 进行任务切换
void task_dispatch(void){
    irq_state_t state = irq_enter_protection();
    task_t * to = task_next_run();
    //要切换的任务与当前的任务不一样，切换
    if(to != task_manager.cur_task){
        task_manager.cur_task = to;
        to->state = TASK_RUNNING;

        task_switch_from_to(task_manager.cur_task, to);     //从前一个任务切换到下一个任务
    }
    irq_leave_protection(state);
}

void task_time_tick(void){
    task_t * curr_task = task_current();

    if(--curr_task->slice_ticks == 0){

        curr_task->slice_ticks = curr_task->time_ticks;      //重新设置时间片
        task_set_block(curr_task);      //将当前进程从就绪队列中移除
        task_set_ready(curr_task);      //再放到就绪队列的最后面

        task_dispatch();
    }

    // 定时中断检查睡眠队列中的任务是否到期
    list_node_t * curr = list_first(&task_manager.sleep_list);
    while(curr){
        list_node_t * next = curr->next;
        task_t * task = list_node_parent(curr, task_t, run_node);
        if(--task->sleep_ticks == 0){
            //从睡眠队列中移除
            task_set_wakeup(task);
            //加入到就绪队列中
            task_set_ready(task);
        }
        curr = next;
    }
    // 如果加入到就绪队列中需要立即运行
    task_dispatch();
}

// 在睡眠队列中待多少个时钟节拍
void task_set_sleep(task_t * task, uint32_t ticks){
    if(ticks == 0){
        return ;
    }
    task->state = TASK_SLEEP;
    task->sleep_ticks = ticks;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}
// 从睡眠队列中移除
void task_set_wakeup(task_t * task){
    list_remove(&task_manager.sleep_list, &task->run_node);
}

void sys_sleep(uint32_t ms){
    // 可能有多个进程加入到睡眠队列中，需要对睡眠队列进行保护
    irq_state_t state = irq_enter_protection();

    //将当前任务从就绪队列中移除
    task_set_block(task_manager.cur_task);

    // 将该任务加入到睡眠队列中，设置睡眠时间,向上取整
    task_set_sleep(task_manager.cur_task, (ms+(OS_TICKS_MS -1))/ OS_TICKS_MS);

    //将就绪队列中的当前任务切换到下一个任务
    task_dispatch();

    irq_leave_protection(state);
}
