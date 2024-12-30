#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "core/memory.h"

void kernel_init (boot_info_t *boot_info){
    ASSERT(boot_info->ram_region_count != 0);
  
    cpu_init();
    log_init();
    memory_init(boot_info);     //对整个内存初始化
    
    irq_init();
    time_init();
    task_mananger_init();  //濞寸姾顕ф慨鐔虹不閿涘嫭鍊為柣鈺兦归崣褔鎯冮崟顐㈢仴濠殿喖顑呯€碉�?
}

void move_to_first_task(void){
    // void first_task_entry(void);
    // first_task_entry();
    task_t * curr = task_current();
    ASSERT(curr != 0);
    tss_t * tss = &(curr->tss);

    //后续从操作系统跳转到应用程序的时候涉及到特权处理级的时候必须使用内联汇�?
    __asm__ __volatile__(
    // ???????????1????????
        // ????????????????????????????????
        "push %[ss]\n\t"			// SS
        "push %[esp]\n\t"			// ESP
        "push %[eflags]\n\t"           // EFLAGS
        "push %[cs]\n\t"			// CS
        "push %[eip]\n\t"		    // ip
        "iret\n\t"::[ss]"r"(tss->ss),  [esp]"r"(tss->esp), [eflags]"r"(tss->eflags),
        [cs]"r"(tss->cs), [eip]"r"(tss->eip));
}

void init_main(void){
    log_printf("Kernel is running.....");
    log_printf("Version: %s %s", OS_VERSION, "diyx86os");
    log_printf("%d %d %x %c",123456, -123, 0x12345, 'a');

    //濞寸姾顕ф慨鐔虹不閿涘嫭鍊為柛锝冨妺閼垫垹绮璺伇濞戞搩浜欓幑銏ゅ礉閳ュ啿鐏ュ┑顔碱儏鐎垫煡骞欏鍕�?
    task_first_init();
    //跳转到应用程序中
    move_to_first_task();

}


