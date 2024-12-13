#ifndef TIME_H
#define TIME_H

//晶体振荡器时钟
#define PIT_OSC_FREQ            1193182
//端口定义：命令和模式
#define PIT_COMMAND_MODE_PORT       0x43
//定时器0的数据端口
#define PIT_CHANNEL0_DATA_PORT      0x40
#define PIT_CHANNEL     (0 << 6)
#define PIT_LOAD_LOHI   (3 << 4)
#define PIT_MODE3      (3 << 1)


void time_init (void);

//中断处理函数
void exception_handler_time (void);

#endif