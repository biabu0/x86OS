#ifndef DEVFS_H
#define DEVFS_H

#include <fs/fs.h>

typedef struct _devfs_type_t{
    const char * name;      //设备类型名称
    int dev_type;           //设备类型
    int file_type;          //该设备对应的文件类型

}devfs_type_t;

#endif