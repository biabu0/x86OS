#ifndef FS_H
#define FS_H

#include "fs/file.h"
#include "tools/list.h"
#include "ipc/mutex.h"

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
    FS_DEVFS,       //设备文件系统
}fs_type_t;


//描述一个特定的文件系统，例如 fat16文件系统,设备文件系统
typedef struct _fs_t{
    char mount_point[FS_MOUNT_SIZE];           ///挂载点名称
    fs_type_t type;
    fs_op_t * op;
    void * data;                //fs_op_t中的函数可能会临时保存一些相关的数据，可以先保存在data中
    int dev_id;                 //设备id; 例如磁盘上的fat16对应的分区
    list_node_t node;           //链表的节点
    mutex_t * mutex;            //互斥锁
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
