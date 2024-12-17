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

void kernel_init (boot_info_t *boot_info){
    ASSERT(boot_info->ram_region_count != 0);
  
    cpu_init();
    log_init();
    irq_init();
    time_init();
    task_mananger_init();  //任务管理相关的初始化
}

static uint32_t init_task_stack[1024];
static task_t init_task;

//涓や釜绋嬪簭搴旇鏈夊悇鑷殑鏍堬紝鍥犱负鍏辩敤鐨勬爤浼氬啿绐侊紝浼氱牬鍧忓彟涓€涓爤鐨勬暟鎹紝鎵€浠ラ渶瑕佷袱涓爤
void init_task_entry(void){
    int count = 0;
    for(;;){
        log_printf("int task: %d", count++);
        sys_sched_yield();
    }
}

void init_main(void){
    log_printf("Kernel is running.....");
    log_printf("Version: %s %s", OS_VERSION, "diyx86os");
    log_printf("%d %d %x %c",123456, -123, 0x12345, 'a');

    //鏍堟槸浠庨珮鍦板潃濮嬪紑濮嬪線涓嬬敓闀跨殑锛屾墍浠ユ爤椤跺湴鍧€鏄爤鐨勬湯灏撅紝涔熷氨鏄爤浣庡湴鍧€锛屼紶鍏?&stack[1024]
    task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1024]);
    //任务管理器中第一个任务初始化操作
    task_first_init();

    int count = 0;
    for(;;){
        log_printf("int main: %d", count++);
        sys_sched_yield();
    }
}
