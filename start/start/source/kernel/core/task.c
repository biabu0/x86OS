#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"


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
    task->tss_sel = tss_sel;        //将该任务的选择子保存起来，切换任务时，需要用到
    return 0;
}

int task_init(task_t * task, uint32_t entry, uint32_t esp){
    ASSERT(task != (task_t *)0);        //初始化，不能0
    //tss_init(task, entry, esp);
    uint32_t * pesp = (uint32_t *)esp;
    if(pesp){
        // first run to_task, need init stack, else error when pop stack
        *(--pesp) = entry;
        *(--pesp) = 0;
        *(--pesp) = 0;
        *(--pesp) = 0;
        *(--pesp) = 0;
        task->stack = (uint32_t *)pesp;
    }

    return 0;
}

void simple_switch(uint32_t **from, uint32_t *to);
void task_switch_from_to (task_t *from, task_t *to){
    //switch_to_tss(to->tss_sel);
    simple_switch(&from->stack, to->stack);
};