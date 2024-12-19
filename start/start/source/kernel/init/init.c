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

void kernel_init (boot_info_t *boot_info){
    ASSERT(boot_info->ram_region_count != 0);
  
    cpu_init();
    log_init();
    irq_init();
    time_init();
    task_mananger_init();  //娴犺濮熺粻锛勬倞閻╃ǹ鍙ч惃鍕灥婵瀵�
}

static uint32_t init_task_stack[1024];
static task_t init_task;
static sem_t sem;

//濞戞挶鍊撻柌婊呯矙鐎ｎ亞纰嶉幖瀛樻椤曟岸寮垫径濠冨€楅柤濂変簽濞堟垿寮介崼顒傜闁搞儳濮崇拹鐔煎礂鏉堚晜鏆忛柣銊ュ閻栥倖瀵煎顒€鏆辩紒鎰筏缁辨繃瀵煎杈╁闁秆冪箰瑜扮喐绋夐埀顒佺▔椤忓懐鍨介柣銊ュ閺嗙喖骞戦鍡欑闁圭鍋撳ù鐘劦濞撳墎鎲版担鐤拡濞戞搩浜濋敓锟�?
void init_task_entry(void){
    int count = 0;
    for(;;){
        //sem_wait(&sem);
        log_printf("int task: %d", count++);
        

        //sys_sleep(500);
        //sys_sched_yield();
    }
}

void init_main(void){
    log_printf("Kernel is running.....");
    log_printf("Version: %s %s", OS_VERSION, "diyx86os");
    log_printf("%d %d %x %c",123456, -123, 0x12345, 'a');

    //闁哄秴鐗婂Σ鍛婄鎼淬劎褰柛锔芥緲濞煎啯鎱ㄧ€ｎ亞纾诲┑顔碱儏缁舵碍绋夌€ｎ剚鏅搁梻鈧捄銊︾暠闁挎稑鏈晶宥嗙閵夛妇鍨藉銈堟硾濠€鎾锤閳ь剟寮伴娑氬灲闁汇劌瀚﹢顖滀焊閹惧懐绀夊☉鏃傚枎濮樸劑寮伴娑氬灲濞达絽楠稿﹢鎾锤閳ь剟鏁嶇仦鑲╃倞閿燂拷??&stack[1024]
    task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1024]);
    //娴犺濮熺粻锛勬倞閸ｃ劋鑵戠粭顑跨娑擃亙鎹㈤崝鈥冲灥婵瀵查幙宥勭稊
    task_first_init();
    //信号量初始化，当前没有信号在里面 
    sem_init(&sem, 0);
    irq_enable_global();
    int count = 0;
    for(;;){
        log_printf("first main: %d", count++);
        //sem_notify(&sem);
        //寤舵椂涓€绉掗挓
        //sys_sleep(1000);
        //sys_sched_yield();
    }
}


