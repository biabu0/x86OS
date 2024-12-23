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
    memory_init(boot_info);     //对整个内存初始化
    log_init();
    irq_init();
    time_init();
    task_mananger_init();  //濞寸姾顕ф慨鐔虹不閿涘嫭鍊為柣鈺兦归崣褔鎯冮崟顐㈢仴濠殿喖顑呯€碉拷
}

static uint32_t init_task_stack[1024];
static task_t init_task;
static sem_t sem;

//婵炴垶鎸堕崐鎾绘煂濠婂懐鐭欓悗锝庝簽绾板秹骞栫€涙ɑ顥嗘い鏇熷哺瀵灚寰勬繝鍐ㄢ偓妤呮煠婵傚绨芥繛鍫熷灴瀵粙宕奸鍌滎槷闂佹悶鍎虫慨宕囨嫻閻旂厧绀傞弶鍫氭櫆閺嗗繘鏌ｉ妸銉ヮ仾闁绘牓鍊栫€电厧顫濋鈧弳杈╃磼閹邦亞绛忕紒杈ㄧ箖鐎电厧顫濇潏鈺侇棃闂佺鍐鐟滄壆鍠愮粙澶愬焵椤掍胶鈻旀い蹇撴噽閸ㄤ粙鏌ｉ妸銉ヮ仾闁哄棛鍠栭獮鎴︻敋閸℃瑧顦梺鍦暯閸嬫挸霉閻橆喖鍔︽繛鎾冲閹茬増鎷呴悿顖楁嫛婵炴垶鎼╂禍婵嬫晸閿燂拷?
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

    //闂佸搫绉撮悧濠偽ｉ崨濠勵浄閹兼番鍔庤ぐ顖炴煕閿旇姤绶叉繛鐓庡暞閹便劎鈧綆浜炵壕璇测攽椤旂⒈鍎忕紒鑸电缁嬪鈧綆鍓氶弲鎼佹⒒閳ь剛鎹勯妸锔炬殸闂佹寧绋戦張顒佹櫠瀹ュ棛顩烽柕澶涘閸ㄨ棄顪冮妶鍫熺【婵犫偓閹绢喖閿ら柍褜鍓熷浼搭敍濞戞艾鐏查梺姹囧妼鐎氼厼锕㈤婊€鐒婇柟鎯ф噽缁€澶娾槈閺冨倸鏋庢慨妯稿姂瀵即顢涘☉姘伈婵炶揪绲芥绋匡耿閹绢喖閿ら柍褜鍓熼弫宥囦沪閼测晝鍊為柨鐕傛嫹??&stack[1024]
    task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1024]);
    //濞寸姾顕ф慨鐔虹不閿涘嫭鍊為柛锝冨妺閼垫垹绮璺伇濞戞搩浜欓幑銏ゅ礉閳ュ啿鐏ュ┑顔碱儏鐎垫煡骞欏鍕▕
    task_first_init();
    //淇″彿閲忓垵濮嬪寲锛屽綋鍓嶆病鏈変俊鍙峰湪閲岄潰 
    sem_init(&sem, 0);
    irq_enable_global();
    int count = 0;
    for(;;){
        log_printf("first main: %d", count++);
        //sem_notify(&sem);
        //瀵よ埖妞傛稉鈧粔鎺楁寭

        //sys_sleep(1000);
        //sys_sched_yield();
    }
}


