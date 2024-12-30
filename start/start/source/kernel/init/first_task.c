#include "core/task.h"
#include "tools/log.h"
int first_task_main(void){
    for(;;){
        //目前这两个函数已经不能使用了，现在应用程序是特权级3，只能通过系统调用来获取操作系统内容
        // log_printf("first task.");
        // sys_sleep(1000);
    }
    return 0;
}