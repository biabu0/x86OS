#ifndef FS_H
#define FS_H

#include <sys/stat.h>
#include "file.h"
#include "tools/list.h"
#include "applib/lib_syscall.h"
#include "ipc/mutex.h"
#include "fs/fatfs/fatfs.h"

#define FS_MOUNT_SIZE 32
struct stat;
struct _fs_t;

typedef struct _fs_op_t{
    int (*mount)(struct _fs_t * fs, int major, int minor);      // 挂载文件系统
    void (*unmount)(struct _fs_t * fs);                          // 卸载文件系统
    int (*open) (struct _fs_t * fs, const char * name, file_t * file); // 打开文件，将打开的文件的相关信息放置在file中
    // 在打开的时候将fs的信息放置到file中，读取的时候也就不需要再传入fs了
    int (*read) (char * buf, int len, file_t * file);       //读取存放的缓冲区buf
    int (*write) (char * buf, int len, file_t * file);
    void (*close) (file_t * file);
    int (*seek) (file_t * file, uint32_t offset, int dir);      //从文件中的某个位置读取
    int (*stat) (file_t * file, struct stat * st);              //newlib库中有定义这个结构
}fs_op_t;

//文件系统类型字段
typedef enum _fs_type_t{
    FS_FAT16,
    FS_DEVFS,       //设备文件系统
}fs_type_t;


//描述一个特定的文件系统，例如 fat16文件系统,设备文件系统
typedef struct _fs_t {
    char mount_point[FS_MOUNT_SIZE];       // 挂载点路径长
    fs_type_t type;              // 文件系统类型

    fs_op_t * op;              // 文件系统操作接口
    void * data;                // 文件系统的操作数据
    int dev_id;                 // 所属的设备

    list_node_t node;           // 下一结点

    // 目前暂时这样设计，可能看起来不好，但是是最简单的方法
    // 这样就不用考虑内存分配的问题
    union {
        fat_t fat_data;         // 文件系统相关数据
    };
    mutex_t * mutex;              // 文件系统操作互斥信号量
}fs_t;



void fs_init(void);


int path_to_num(const char * path, int * num);
const char * path_next_child(const char * path);

//可变参数的形式
int sys_open(const char * name, int flags, ...);
int sys_read(int file, char * ptr, int len);
int sys_write(int file, char * ptr, int len);
int sys_lseek(int file, int ptr, int dir);
int sys_close(int file);

int sys_close(int file);

int sys_fstat(int file, struct stat *st);
int sys_isatty(int file);
int sys_dup(int file);

#endif
