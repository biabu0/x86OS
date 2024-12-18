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
    task_mananger_init();  //浠诲姟绠＄悊鐩稿叧鐨勫垵濮嬪寲
}

static uint32_t init_task_stack[1024];
static task_t init_task;

//娑撱倓閲滅粙瀣碍鎼存棁顕氶張澶婃倗閼奉亞娈戦弽鍫礉閸ョ姳璐熼崗杈╂暏閻ㄥ嫭鐖ゆ导姘暱缁愪緤绱濇导姘辩壃閸у繐褰熸稉鈧稉顏呯垽閻ㄥ嫭鏆熼幑顕嗙礉閹碘偓娴犮儵娓剁憰浣疯⒈娑擃亝锟�?
void init_task_entry(void){
    int count = 0;
    for(;;){
        log_printf("int task: %d", count++);
        sys_sleep(500);
        //sys_sched_yield();
    }
}

void init_main(void){
    log_printf("Kernel is running.....");
    log_printf("Version: %s %s", OS_VERSION, "diyx86os");
    log_printf("%d %d %x %c",123456, -123, 0x12345, 'a');

    //閺嶅牊妲告禒搴ㄧ彯閸︽澘娼冩慨瀣磻婵绶氭稉瀣晸闂€璺ㄦ畱閿涘本澧嶆禒銉︾垽妞よ泛婀撮崸鈧弰顖涚垽閻ㄥ嫭婀亸鎾呯礉娑旂喎姘ㄩ弰顖涚垽娴ｅ骸婀撮崸鈧敍灞肩炊锟�??&stack[1024]
    task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1024]);
    //浠诲姟绠＄悊鍣ㄤ腑绗竴涓换鍔″垵濮嬪寲鎿嶄綔
    task_first_init();
    irq_enable_global();
    int count = 0;
    for(;;){
        log_printf("first main: %d", count++);
        //延时一秒钟
        sys_sleep(1000);
        //sys_sched_yield();
    }
}


