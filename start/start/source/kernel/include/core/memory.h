#ifndef MEMORY_H
#define MEMORY_H

#include "comm/types.h"
#include "tools/bitmap.h"
#include "ipc/mutex.h"
#include "comm/boot_info.h"

#define MEM_EXT_START (1024*1024)
#define MEM_EXT_END   0x80000000
#define MEM_PAGE_SIZE 4096

#define MEM_EBDA_START  0x80000

#define MEMORY_TASK_START   (127 * 1024 * 1024)
//对地址进行分配，从整个内存中取地址，找到一个空闲的内存块的地址
//该功能可能被很多进程或者任务使用，临界资源
typedef struct _addr_alloc_t {
    mutex_t mutex;      //互斥量  
    bitmap_t bitmap;
    uint32_t start; //起始地址
    uint32_t size;  //总大小
    uint32_t page_size;     //页的大小
}addr_alloc_t;

//虚拟内存映射
typedef struct _memory_map_t{
    void * vstart;      //虚拟起始地址
    void * vend;        //虚拟终止地址
    void * pstart;  //物理起始地址
    uint32_t perm;       //权限
}memory_map_t;

void memory_init(boot_info_t * boot_info);
uint32_t memory_create_uvm(void);
#endif