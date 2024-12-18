#include "dev/time.h"
#include "comm/types.h"
#include "cpu/irq.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "core/task.h"

//定时器计数
static uint32_t sys_tick;


//定时中断处理函数
void do_handler_time(exception_frame_t * frame){
    sys_tick++;
    //调用下面的函数通知8259可以继续相应后序的中断
    pic_send_eoi(IRQ0_TIMER);
    // 必须放到后面
    task_time_tick();
}

static void init_pit (void){

    //定时器需要加载的数值
    uint32_t reload_count = PIT_OSC_FREQ * OS_TICKS_MS / 1000;

    outb(PIT_COMMAND_MODE_PORT, PIT_CHANNEL | PIT_LOAD_LOHI | PIT_MODE3);
    outb(PIT_CHANNEL0_DATA_PORT, reload_count & 0xFF);
    outb(PIT_CHANNEL0_DATA_PORT, (reload_count >> 8) & 0xFF);

    irq_install(IRQ0_TIMER, (irq_handler_t)exception_handler_time);
    irq_enable(IRQ0_TIMER);
}

void time_init (void){
    sys_tick = 0;
    init_pit();
}
