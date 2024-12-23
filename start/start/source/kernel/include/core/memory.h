#ifndef MEMORY_H
#define MEMORY_H

#include "comm/types.h"
#include "tools/bitmap.h"
#include "ipc/mutex.h"
#include "comm/boot_info.h"

//对地址进行分配，从整个内存中取地址，找到一个空闲的内存块的地址
//该功能可能被很多进程或者任务使用，临界资源
typedef struct _addr_alloc_t {
    mutex_t mutex;      //互斥量  
    bitmap_t bitmap;
    uint32_t start; //起始地址
    uint32_t size;  //总大小
    uint32_t page_size;     //页的大小
}addr_alloc_t;

void memory_init(boot_info_t * boot_info);

#endif