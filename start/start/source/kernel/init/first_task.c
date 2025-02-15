#include "core/task.h"
#include "tools/log.h"
#include "applib/lib_syscall.h"
#include "dev/tty.h"
int first_task_main(void){
#if 0
    //int pid = getpid();
    int count = 100;
    int pid = fork();
    if(pid < 0){
        print_msg("error: fork failed.%d", pid);
    }else if(pid == 0){
        print_msg("child : %d", pid);
        char * argv[] = {"arg0", "arg1", "arg2", "arg3"};
        //目前没有文件系统，放一个假的
        execve("/shell.elf", argv, (char **)0);
    }else{
        print_msg("parent pid: %d", pid);
    }
#endif
    for(int i = 0; i < TTY_NR; i++){
        int pid = fork();
        if(pid < 0){
            print_msg("create shell failed.", 0);
            break;
        }else if(pid == 0){
            char tty_num[5] = "tty:?";
            tty_num[4] = i + '0';
            char * argv[] = {tty_num, (char *)0};
            execve("/shell.elf", argv, (char **)0);
            while(1){
                msleep(1000);
            }
        }
    }
    for(;;){
        //目前这两个函数已经不能使用了，现在应用程序是特权级3，只能通过系统调用来获取操作系统内容
        // log_printf("first task.");
        // sys_sleep(1000);
        //print_msg("task id = %d", pid);
        int status;
        wait(&status);
    }
    return 0;
}