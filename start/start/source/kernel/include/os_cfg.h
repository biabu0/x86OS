#ifndef OS_CFG_H
#define OS_CFG_H

// #define 
#define GDT_TABLE_SIZE 256
//绗?竴涓?〃椤圭殑鍋忕Щ鏄?8涓?瓧鑺?
#define KERNEL_SELECTOR_CS (1 * 8)          //8，传入后右移3位，是0x1，索引就是1，则位于第一个段描述符
#define KERNEL_SELECTOR_DS (2 * 8)          //第二个段描述符
#define SELECTOR_SYSCALL   (3 * 8)
#define KERNEL_STACK_SIZE (8*1024)

#define IDLE_TASK_STACK_SIZE 1024       //空闲进程的栈大小
#define OS_TICKS_MS             10
#define OS_VERSION          "1.0.0"

//整个系统中task的数量
#define TASK_NR             128

#endif