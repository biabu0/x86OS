#ifndef MAIN_H
#define MAIN_H

// command line interface
#define CLI_INPUT_SIZE      1024
#define CLI_MAX_ARG_COUNT   10

//后面只有两个字符的CSI的序列
#define ESC_CMD2(Pn, cmd)                   "\x1b["#Pn#cmd
#define ESC_CLEAR_SCREEN                    ESC_CMD2(2,J)
#define ESC_COLOR_ERROR                     ESC_CMD2(31, m)
#define ESC_COLOR_DEFAULT                   ESC_CMD2(39, m)
#define ESC_MOVE_CURSOR(row, col)           "\x1b["#row";"#col"H" 


typedef struct _cli_cmd_t{
    const char * name;          //命令名称echo,clear等
    const char * usage;         //命令使用说明
    int (*do_func)(int argc, char ** argv);//具体执行什么功能，函数指针
}cli_cmd_t;
typedef struct _cli_t{
    char curr_input[CLI_INPUT_SIZE];        //定义一个shell的输入缓冲区
    const cli_cmd_t * cmd_start;            //cli命令表的起始与结束
    const cli_cmd_t * cmd_end;
    const char * promot;                    //指向一个特定的字段，作为shell的提示符打印出来
}cli_t;


#endif