#include "lib_syscall.h"
#include <stdio.h>
#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/file.h>

char cmd_buf[256];
static cli_t cli;       //命令行解释器
static const char * promot = "sh >>";            //提示字符串

static int do_help(int argc, char **argv){
    cli_cmd_t * start = cli.cmd_start;
    while(start < cli.cmd_end){
        printf("%s, %s\n", start->name, start->usage);
        start++;
    }
    return 0;
}

static int do_clear(int argc, char * argv[]){
    //转义序列清屏
    printf("%s", ESC_CLEAR_SCREEN);
    printf("%s", ESC_MOVE_CURSOR(0,0));
    return 0;
}

static int do_echo(int argc, char * argv[]){
    //只输入echo
    if(argc == 1){
        char msg_buf[128];
        fgets(msg_buf, sizeof(msg_buf), stdin);
        msg_buf[sizeof(msg_buf) - 1] = '\0';
        puts(msg_buf);
        return 0;
    }

    int count = 1; //没有-n参数 默认打印一遍
    int ch;
    //找带参数的-n和不带参数-h
    while((ch = getopt(argc, argv, "n:h")) != -1){
        switch(ch){
            case 'n':
                count = atoi(optarg);           //-n后面的参数会放到optarg中；assic 转 整数-> atoi
                break;
            case 'h':
                printf("Usage: echo [-n count] msg\n");
                optind = 1;                         //optind是一个全局变量，每次使用完之后要重新设置为1
                return 0;
            case '?':           //不认识的参数
                if(optarg){
                    fprintf(stderr, "%s", ESC_COLOR_ERROR);     //红色
                    fprintf(stderr, "Unknown option: -%c\n", optarg);
                    fprintf(stderr, "%s", ESC_COLOR_DEFAULT);       //恢复默认颜色
                }
                optind = 1; 
                return -1;
            default:
                optind = 1; 
                return 0;
        }
    }
    if(optind > argc - 1){
        fprintf(stderr, "%s", ESC_COLOR_ERROR);     //红色
        fprintf(stderr, "Message is emptyn");
        fprintf(stderr, "%s", ESC_COLOR_DEFAULT);       //恢复默认颜色
        optind = 1; 
        return -1;
    }
    char * msg = argv[optind];
    for(int i = 0; i < count; i++){
        puts(msg);
    }
    optind = 1; 
    return 0;   
}


static int do_exit(int argc, char ** argv){
    exit(0);//参数会返回给操作系统退出码 0 通常表示程序成功执行完毕。
            //非零退出码通常表示程序遇到了错误或异常情况。不同的非零值可以用来表示不同的错误类型或状态。
    return 0;       //执行不到，但还是要加，不然可能编译错误
}

//命令表
static const cli_cmd_t cmd_list[] = {
    {
        .name = "help",
        .usage = "help -- list supported commands",
        .do_func = do_help,
    },
    {
        .name = "clear",
        .usage = "clear -- clear screen",
        .do_func = do_clear,
    },
    {
        .name = "echo",
        .usage = "echo [-n count] msg  -- echo something",
        .do_func = do_echo,
    },
    {
        .name = "quit",
        .usage = "quit -- quit from shell",
        .do_func = do_exit,
    },

};


static void show_promot(){
    //不能使用puts(cli.promot)会加上一个回车，导致打印提示符后光标移动到下一行
    printf("%s",cli.promot);//不加回车会将字符串缓存在newlib内部，不会立即输出字符串
    fflush(stdout);         //刷新缓冲区
}
static void cli_init(const char * promot, const cli_cmd_t * cmd_list, int size){
    cli.promot = promot;
    memset(cli.curr_input, 0, CLI_INPUT_SIZE);
    cli.cmd_start = cmd_list;
    cli.cmd_end = cmd_list + size;
}


static cli_cmd_t * find_builtin(const char * name){
    for(cli_cmd_t * cmd = cli.cmd_start; cmd < cli.cmd_end; cmd++){
        if(strcmp(cmd->name, name) != 0){
            continue;
        }
        return cmd;
    }
    return (cli_cmd_t *)0;
}

static void run_builtin(cli_cmd_t * cmd, int argc, char ** argv){
    int ret = cmd->do_func(argc, argv);
    if(ret < 0){
        fprintf(stderr, "error: %d\n", ret);
    }
}

//shell - ls - ls0, ls1
//shell - ls0, ls1这里ls进程退出，但其子进程存在，为孤儿进程；

static void run_exec_file(const char * path, int argc, char ** argv){
    int pid = fork();
    if(pid < 0){
        fprintf(stderr, "fork failed %s", path);
    }else if(pid == 0){
        for(int i = 0; i < argc; i++){
            printf("arg %d = %s\n", i, argv[i]);
        }
        exit(-1);           //子进程执行结束,将-1给到status中
    }else{
        int status;
        int pid = wait(&status);      //等待任何子进程的退出，将错误码设置到status，返回子进程的pid
        fprintf(stderr, "cmd %s result: %d, pid=%d\n", path, status, pid);
    }
}

int main (int argc, char **argv){
    open(argv[0], O_RDWR);               //打开tty设备，返回的id是0   stdin, 读写方式
    dup(0);                         //1   stdout        
    //dup 函数用于复制一个现有的文件描述符，返回一个新的文件描述符，这个新的文件描述符与原文件描述符指向同一个文件、管道或设备。
    dup(0);               //2   stderr

    puts("Hello from x86 os");
    printf("OS version : %s\n", "1.0.0");

    cli_init(promot, cmd_list, sizeof(cmd_list)/sizeof(cmd_list[0]));
    for(;;){
        show_promot();          //打印提示符
        //gets(cli.curr_input);           //没有限制用户输入的数据量，可能超出缓冲区
        char * str = fgets(cli.curr_input, CLI_INPUT_SIZE, stdin);          //得到的字符串会包含换行符\r和\n
        if(str == NULL){
            printf("shell input err.\n");
            continue;
        }
        
        char * nl = strchr(cli.curr_input, '\n');
        if(nl){                 //如果字符串中包含\n，则删除\n
            *nl = '\0';
        }
        char * cr = strchr(cli.curr_input, '\r');
        if(cr){
            *cr = '\0';
        }
        
        int argc = 0;                       //存放shell命令的参数个数以及参数
        char * argv[CLI_MAX_ARG_COUNT];
        memset(argv, 0, sizeof(argv));

        const char * space = " ";               //命令行中使用空格分割
        char * token = strtok(cli.curr_input, space);
        while(token){
            argv[argc++] = token;
            token = strtok(NULL, space);
        }

        //没有任何字符串
        if(argc == 0){
            continue;
        }

        cli_cmd_t * cmd = find_builtin(argv[0]);
        if(cmd){
            run_builtin(cmd, argc, argv);
            continue;
        }


        run_exec_file("",argc, argv);        //没有从内置命令中找到相应的命令，查找可执行程序


        fprintf(stderr, "%s", ESC_COLOR_ERROR);     //红色
        fprintf(stderr, "Unknown command: %s\n", cli.curr_input);
        fprintf(stderr, "%s", ESC_COLOR_DEFAULT);       //恢复默认颜色

        //fprintf(stderr, ESC_COLOR_ERROR"Unknown command: %s\n"ESC_COLOR_DEFAULT, cli.curr_input);
        

        //  exec磁盘加载


        
        // gets(cmd_buf);//从标准输入里面读取一个字符串->会调用sys_read读取
        // puts(cmd_buf);//输出到标准输出->会调用sys_write写入
        // printf("shell pid = %d\n", getpid());
        // msleep(1000);
    };
}