#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"
int first_task_main(void){
    //int pid = getpid();
    int count = 100;
    int pid = fork();
    if(pid < 0){
        print_msg("error: fork failed.%d", pid);
    }else if(pid == 0){
        print_msg("child : %d", pid);
    }else{
        print_msg("parent pid: %d", pid);
    }
    for(;;){
        //目前这两个函数已经不能使用了，现在应用程序是特权级3，只能通过系统调用来获取操作系统内容
        // log_printf("first task.");
        // sys_sleep(1000);
        print_msg("task id = %d", pid);
        msleep(1000);
    }
    return 0;
}