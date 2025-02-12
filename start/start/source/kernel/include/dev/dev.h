#ifndef DEV_H
#define DEV_H

#define DEV_NAME_SIZE           32
enum{
    DEV_UNKONWN = 0,
    DEV_TTY,
};
//某一种类型下的特定某种设备
typedef struct _device_t{
    struct _dev_desc_t * desc;

    int mode;                   //设备支持的模式，只读还是读写等特性？
    int minor;                  //次设备号: 哪一块磁盘，哪一个定时器？
    void * data;                //设备相关参数
    int open_count;             //打开次数
}device_t;

//设备描述结构体，描述某一种类型的设备提供的接口和特性
typedef struct _dev_desc_t {
    char name[DEV_NAME_SIZE];       //设备名称
    int major;                      //对应的设备类型（硬盘、显示器等）
    //操作系统只需要调用这个open即可
    int (*open)(device_t *dev);     //函数指针，指向设备的打开函数
    int (*read)(device_t * dev, int addr, char * buf, int size);
    int (*write)(device_t * dev, int addr, char * buf, int size);
    //设备特定的操作
    int (*control)(device_t * dev, int cmd, int arg0, int arg1);
    void (*close)(device_t * dev);
}dev_desc_t;

int dev_open(int major, int minor, void * data);
int dev_read(int dev_id, int addr, char *buf, int size);
int dev_write(int dev_id, int addr, char *buf, int size);
int dev_control(int dev_id, int cmd, int arg0, int arg1);
void dev_close(int dev_id);

#endif
