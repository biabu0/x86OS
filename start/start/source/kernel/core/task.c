#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/mmu.h"
static uint32_t idle_task_stack[IDLE_TASK_STACK_SIZE];
// 整个系统中只需要一个任务管理器，定义为全局变量
static task_manager_t task_manager;
static int tss_init(task_t * task, int flag, uint32_t entry, uint32_t esp){
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
    uint32_t kernel_stack = memory_alloc_page();
    if(kernel_stack == 0){
        goto tss_init_failed;
    }
    int code_sel, data_sel;
    if(flag & TASK_FLAGS_SYSTEM){
        code_sel = KERNEL_SELECTOR_CS;
        data_sel = KERNEL_SELECTOR_DS;
    }else{
        code_sel = task_manager.app_code_sel | SEG_CPL3;
        data_sel = task_manager.app_data_sel | SEG_CPL3;
    }

    task->tss.eip = entry;      //CPU指令的位置，第一次运行时，将函数地址给eip保存起来
    task->tss.esp = esp;       //程序栈顶指针，将分配给进程的10页空间剩余的部分作为esp3的栈
    task->tss.esp0 = kernel_stack  + MEM_PAGE_SIZE; //esp0跟特权级有关，运行在特权级0, 单独分配栈空间，用于中断，系统调用等
    task->tss.ss = data_sel;      //平坦模型，数据段，也会用作栈空间
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.es = task->tss.ds = task->tss.fs = task->tss.gs = data_sel;
    task->tss.cs = code_sel;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;      //IF为1，我们不希望从TSS恢复后所有的中断不能响应
    //其他通用寄存器，设置为0，里面的值由程序自己设置
    //cr3，后序课程使用
    uint32_t page_dir = memory_create_uvm();
    if(page_dir == 0){
        goto tss_init_failed;
    }
    task->tss.cr3 = page_dir;//将页表设置到cr3
    task->tss_sel = tss_sel;        //将该任务的选择子保存起来，切换任务时，需要用到
    return 0;
tss_init_failed:
    //页表创建失败，释放描述符
    gdt_free_sel(tss_sel);
    if(kernel_stack){
        memory_free_page(kernel_stack);
    }
    return -1;
}


int task_init(task_t * task, const char * name, int flag, uint32_t entry, uint32_t esp){
    ASSERT(task != (task_t *)0);        //初始化，不能0
    tss_init(task, flag, entry, esp); //初始化了tss和tss_sel

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
    void first_task_entry (void);
    extern uint8_t s_first_task[], e_first_task[];
    //程序数据，但不能只分配这么多，还要有栈等
    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;//分配10页的大小，first_task只是简单初始化一些程序，不会跑应用程序
    ASSERT(copy_size < alloc_size);

    uint32_t first_start = (uint32_t)first_task_entry;

    //first_task分配的10页大小的位置中其余部分作为栈空间,将其设置到特权级3的位置
    task_init(&task_manager.first_task, "first task", 0, first_start, first_start + alloc_size);
    write_tr(task_manager.first_task.tss_sel);
    task_manager.cur_task = &task_manager.first_task;

    //在进行任务初始化的时候里面的tss初始化为该任务分配了一个页表放到了CR3中，使用该函数完成了页表的切换，此时，页表就是自己的页表
    //由于操作系统的代码依然在任务代页表中，并不会出现问题
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    //设置完页表后，给自己分配空间first_start为0x80000000，分配了10页，属性可写（将.data、.text等放在了一起）
    memory_alloc_page_for(first_start, alloc_size, PTE_P | PTE_W | PTE_U);
    //从物理地址开始copy到0x80000000，拷贝copy_size大小（实际大小）
    kernel_memcpy((void *)first_start, s_first_task, copy_size);
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
    int sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_TYPE_DATA | SEG_S_NORMAL | SEG_D | SEG_TYPE_RW
    );
    task_manager.app_data_sel = sel;

    sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_TYPE_CODE | SEG_S_NORMAL | SEG_D | SEG_TYPE_RW
    );
    task_manager.app_code_sel = sel;
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    task_manager.cur_task = (task_t *)0;

    task_init(&task_manager.idle_task, 
        "idle task", 
        TASK_FLAGS_SYSTEM,
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
