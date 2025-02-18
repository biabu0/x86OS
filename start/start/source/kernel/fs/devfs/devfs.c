#include "fs/devfs/devfs.h"
#include "dev/dev.h"
#include "tools/klib.h"
#include "tools/log.h"

static devfs_type_t dev_type_list[] = {
    {
        .name = "tty",
        .dev_type = DEV_TTY,
        .file_type = FILE_TTY,
    },
};
// 挂载文件系统
int devfs_mount(struct _fs_t * fs, int major, int minor){
    //其他的在上一层代码中实现
    fs->type = FS_DEVFS;
}     
void devfs_unmount(struct _fs_t * fs){

}
int devfs_open(struct _fs_t * fs, const char * path, file_t * file){
    // tty0, tty1
    //设备文件系统中能够支持的设备相关的信息
    for(int i = 0; i < sizeof(dev_type_list)/sizeof(dev_type_list[0]); i++){
        devfs_type_t * type = dev_type_list + i;
        int type_name_len = kernel_strlen(type->name);
        if(kernel_strncmp(path, type->name, type_name_len) == 0){
            int minor;
            if((kernel_strlen(path) > type_name_len) && (path_to_num(path + type_name_len, &minor) < 0)){
                log_printf("Get device num failed. %s", path);
                break;
            }
            int dev_id = dev_open(type->dev_type, minor, (void *)0);
            if(dev_id < 0){
                log_printf("Open device failed. %s", path);
                break;
            }
            //对文件信息进行设置
            file->dev_id = dev_id;
            // 将文件系统的信息放置到打开的文件中
            file->fs = fs;
            file->pos = 0;
            file->size = 0;
            file->type = type->file_type;
            return 0;
        }
    }
    return 0;
}
// 在打开的时候将fs的信息放置到file中，读取的时候也就不需要再传入fs了
int devfs_read(char * buf, int len, file_t * file){
    return dev_read(file->dev_id, file->pos, buf, len);
}
int devfs_write(char * buf, int len, file_t * file){
    return dev_write(file->dev_id, file->pos, buf, len);

}
void devfs_close(file_t * file){
    dev_close(file->dev_id);
}
int devfs_seek(file_t * file, uint32_t offset, int dir){
    return -1;
}
int devfs_stat(file_t * file, struct stat * st){
    return -1;
}

fs_op_t devfs_op = {
    .mount = devfs_mount,
    .unmount = devfs_unmount,
    .open = devfs_open,
    .read = devfs_read,
    .write = devfs_write,
    .close = devfs_close,
    .seek = devfs_seek,
    .stat = devfs_stat
};