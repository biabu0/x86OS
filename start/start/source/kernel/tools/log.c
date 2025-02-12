#include <stdarg.h>
#include "tools/log.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "ipc/mutex.h"
#include "dev/console.h"
#include "dev/dev.h"

static mutex_t mutex;


#define LOG_USE_COM     0
//第一个串行接口的起始地址
#define COM1_PORT     0x3F8


static int log_dev_id;

//初始化串行接口，硬件相关的初始化工作,无需过多了解
void log_init(void){
    mutex_init(&mutex);
    //只需要修改设备号即可，其他不需要修改就可以向不同的设备中进行写
    log_dev_id = dev_open(DEV_TTY, 0, (void*)0);
#if LOG_USE_COM
    outb(COM1_PORT+1, 0x00);
    outb(COM1_PORT+3, 0x80);
    outb(COM1_PORT+0, 0x3);
    outb(COM1_PORT+1, 0x00);
    outb(COM1_PORT+3, 0x03);
    outb(COM1_PORT+2, 0xc7);
    outb(COM1_PORT+4, 0x0F);
#endif

}

//printf接口函数    ...可变参数，参数数量没有限,stdarg.h库可以处理...
void log_printf(const char * fmt, ...){
    char str_buf[128];
    va_list args;

    kernel_memset(str_buf, '\0', sizeof(str_buf));     //清空缓冲区
    va_start(args, fmt);
    kernel_vsprintf(str_buf, fmt, args);
    va_end(args);
    
    //进入临界区保护模式,要注意恢复现场，使用state变量保存进入临界区之前的中断状态
    mutex_locK(&mutex);
#if LOG_USE_COM
    const char * p = str_buf;
    while(*p != '\0'){
        while((inb(COM1_PORT + 5) & (1 << 6)) == 0);//检查串口是否忙，忙则等待
        outb(COM1_PORT, *p++);
    }

    //换行
    outb(COM1_PORT, '\r');
    outb(COM1_PORT, '\n');
#else
    //屏幕打印
    //console_write(0, str_buf, kernel_strlen(str_buf));
    dev_write(log_dev_id, 0, str_buf, kernel_strlen(str_buf));
    char c = '\n';
    //console_write(0, &c, 1);
    dev_write(log_dev_id, 0, &c, 1);
#endif
    //退出临界区保护模式，恢复之前的中断状态
    mutex_unlock(&mutex);
}
