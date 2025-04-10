#include "fs/fs.h"
#include "fs/file.h"
#include "comm/types.h"
#include "tools/klib.h"
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"
#include "tools/log.h"

#include "dev/disk.h"
#include <sys/stat.h>
#include "dev/console.h"
#include "dev/dev.h"
#include "core/task.h"
#include "tools/log.h"
#include <sys/file.h>
#include "fs/fatfs/fatfs.h"
#include "os_cfg.h"

//最多支持10个文件系统节点加入到链表中
#define FS_TABLE_SIZE 10
//定义一个链表，放置已经初始化好的文件系统类型，放的是fs_t类型的结构
static list_t mounted_list;
static fs_t fs_table[FS_TABLE_SIZE];
// 将10个文件系统加入到空闲链表中，当执行挂在操作的时候，则将相应的文件系统加入到挂载链表中
static list_t free_list;

extern fs_op_t devfs_op;
extern fs_op_t fatfs_op;

static fs_t * root_fs;

static uint8_t TEMP_ADDR[100*1024];
static uint8_t * temp_pos;

#define TEMP_FILE_ID        100
//直接从磁盘上对应的位置（5000个扇区的位置）
static void read_disk(int sector, int sector_count, uint8_t * buf) {
    outb(0x1F6, (uint8_t) (0xE0));

	outb(0x1F2, (uint8_t) (sector_count >> 8));
    outb(0x1F3, (uint8_t) (sector >> 24));		// LBA参数的24~31位
    outb(0x1F4, (uint8_t) (0));					// LBA参数的32~39位
    outb(0x1F5, (uint8_t) (0));					// LBA参数的40~47位

    outb(0x1F2, (uint8_t) (sector_count));
	outb(0x1F3, (uint8_t) (sector));			// LBA参数的0~7位
	outb(0x1F4, (uint8_t) (sector >> 8));		// LBA参数的8~15位
	outb(0x1F5, (uint8_t) (sector >> 16));		// LBA参数的16~23位

	outb(0x1F7, (uint8_t) 0x24);

	// 读取数据
	uint16_t *data_buf = (uint16_t*) buf;
	while (sector_count-- > 0) {
		// 每次扇区读之前都要检查，等待数据就绪
		while ((inb(0x1F7) & 0x88) != 0x8) {}

		// 读取并将数据写入到缓存中
		for (int i = 0; i < SECTOR_SIZE / 2; i++) {
			*data_buf++ = inw(0x1F0);
		}
	}
}

static int is_path_vaild(const char * path){
    if((path == (const char * )0) || (path[0] == '\0')){
        return 0;
    }
    return 1;
}

static int is_fd_bad(int file){
    if((file < 0) && (file >= TASK_OFILE_NR)){
        return 1;
    }
    return 0;
}

static void fs_protect(fs_t *fs){
    if(fs->mutex){
        mutex_locK(fs->mutex);
    }
}
static void fs_unprotect(fs_t *fs){
    if(fs->mutex){
        mutex_unlock(fs->mutex);
    }
}


int path_begin_with(const char * path, const char * str){
    const char * s1 = path, *s2 = str;
    while(*s1 && *s2 && (*s1 == *s2)){
        s1++;
        s2++;
    }
    return *s2 == '\0';
}
//  /dev/  /home/   需要根据路径，取前面的字符串，从mounted_list中找相应的名称匹配的结构
int sys_open(const char * name, int flags, ...){
    //shell文件的加载
    if(kernel_strncmp(name, "/shell.elf", 3) == 0){
        int dev_id = dev_open(DEV_DISK, 0xa0, (void *)0);  //0xa0，是第0个分区，描述的是整个磁盘
        dev_read(dev_id, 5000, (uint8_t *)TEMP_ADDR, 80);
        // read_disk(5000, 80, (uint8_t *)TEMP_ADDR);      //从第5000个扇区开始读到内存中
        temp_pos = TEMP_ADDR;
        return TEMP_FILE_ID;
    }

    // /dev/tty
    file_t * file = file_alloc();
    if(!file){
        return -1;
    }
    int fd = task_alloc_fd(file);
    if(fd < 0){
        goto sys_open_failed;
    }

    //从路径里面解析，找到对应的fs结构
    fs_t * fs = (fs_t *)0;
    list_node_t * node = list_first(&mounted_list);
    while(node){
        fs_t * curr = list_node_parent(node, fs_t, node);
        if(path_begin_with(name, curr->mount_point)){
            fs = curr;
            break;
        }
        node = list_node_next(node);
    }
    if(fs){
        name = path_next_child(name);   //  从/dev/tty0中获得tty0
    }else{
        fs = root_fs;
        //给一个缺省的
    }

    file->mode = flags;         //newlib会将打开的模式传递到flags标志位
    file->fs = fs;
    kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);

    fs_protect(fs);       //互斥信号量进行保护
    // 使用更加通用的打开方式，文件系统进行上锁，一次只能允许一个进程操作
    int err = fs->op->open(fs, name, file);
    if(file < 0){
        fs_unprotect(fs);
        log_printf("open %s failed", name);
        goto sys_open_failed;
    }
    fs_unprotect(fs);
    return fd;
sys_open_failed:
    file_free(file);
    if(fd >= 0){
        task_remove_fd(fd);
    }
    return -1;
}
int sys_read(int file, char * ptr, int len){
    //shell的处理
    if(TEMP_FILE_ID == file){
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    }
    if(is_fd_bad(file) || !ptr || !len){
        return 0;
    }

    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }
    //检查文件的读写模式
    if(p_file->mode == O_WRONLY){
        log_printf("file is write only");
        return -1;
    }
    fs_t * fs = p_file->fs;
    fs_protect(fs);
    int err = fs->op->read(ptr, len, p_file);
    fs_unprotect(fs);
    return err;
}


int sys_write(int file, char * ptr, int len){
    // // 如果文件是标准输出，通过串口实现；希望printf函数的输出，能够显示到计算机的屏幕上，涉及显示器的处理
    // if(file == 1){
    //     ptr[len] = '\0';
    //     //console_write(0, ptr, len);     // 显示到屏幕上
    //     log_printf("%s",ptr);         // 显示到串口上
    // }

    // file_t * p_file = task_file(file);
    // if(!p_file){
    //     log_printf("file not opened");
    //     return -1;
    // }
    // return dev_write(p_file->dev_id, 0, ptr, len);

    if(is_fd_bad(file) || !ptr || !len){
        return 0;
    }

    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }
    //检查文件的读写模式
    if(p_file->mode == O_RDONLY){
        log_printf("file is read only");
        return -1;
    }
    fs_t * fs = p_file->fs;
    fs_protect(fs);
    int err = fs->op->write(ptr, len, p_file);
    fs_unprotect(fs);
    return err;
}
int sys_lseek(int file, int ptr, int dir){
    if(file == TEMP_FILE_ID){
        //简单地移动指针，偏移量为据开始的位置
        temp_pos = (uint8_t *)(TEMP_ADDR + ptr);
        return 0;
    }

    if(is_fd_bad(file)){
        return 0;
    }

    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }

    fs_t * fs = p_file->fs;
    fs_protect(fs);
    int err = fs->op->seek(p_file, ptr, dir);
    fs_unprotect(fs);
    return err;
} 
int sys_close(int file){
    if(file == TEMP_FILE_ID){
        return 0;
    }
    if(is_fd_bad(file)){
        log_printf("file %d is not valid.", file);
        return -1;
    }
    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }
    ASSERT(p_file->ref > 0);
    // 文件打开多次，则减少引用，减少引用为0，则关闭文件
    if(p_file->ref-- == 1){
        fs_t * fs = p_file->fs;
        fs_protect(fs);
        fs->op->close(p_file);
        fs_unprotect(fs);

        file_free(p_file);  
    }
    task_remove_fd(file);
    return 0;
}

int sys_fstat(int file, struct stat *st){
    if(is_fd_bad(file)){
        return -1;
    }
    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }

    fs_t * fs = p_file->fs;
    kernel_memset(st, 0, sizeof(struct stat));

    fs_protect(fs);
    int err = fs->op->stat(p_file, st);
    fs_unprotect(fs);
    return err;
}
int sys_isatty(int file){
    if(is_fd_bad(file)){
        return -1;
    }
    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }

    return p_file->type == FILE_TTY;
}

int sys_dup(int file){
    if(is_fd_bad(file)){
        log_printf("file %d is not valid.", file);
        return -1;
    }
    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened.");
        return -1;
    }
    int fd = task_alloc_fd(p_file);
    if(fd >= 0){
        file_inc_ref(p_file);
        return fd;
    }
    log_printf("no task file avaliable");
    return -1;
}

static fs_op_t * get_fs_op(fs_type_t type, int major){
    switch(type){
        case FS_FAT16:
            return &(fatfs_op);
        case FS_DEVFS:
            return &(devfs_op);
        default:
            return (fs_op_t *)0;
    }
}

//mount_point：挂载点，用于从链表中找相应的结构
static fs_t * mount (fs_type_t type, char * mount_point, int dev_major, int dev_minor) {
	fs_t * fs = (fs_t *)0;

	log_printf("mount file system, name: %s, dev: %x", mount_point, dev_major);

	// 遍历，查找是否已经有挂载
 	list_node_t * curr = list_first(&mounted_list);
	while (curr) {
		fs_t * fs = list_node_parent(curr, fs_t, node);
		if (kernel_strncmp(fs->mount_point, mount_point, FS_MOUNT_SIZE) == 0) {
			log_printf("fs alreay mounted.");
			goto mount_failed;
		}
		curr = list_node_next(curr);
	}

	// 分配新的fs结构
	list_node_t * free_node = list_remove_first(&free_list);
	if (!free_node) {
		log_printf("no free fs, mount failed.");
		goto mount_failed;
	}
	fs = list_node_parent(free_node, fs_t, node);

	// 检查挂载的文件系统类型：不检查实际
	fs_op_t * op = get_fs_op(type, dev_major);
	if (!op) {
		log_printf("unsupported fs type: %d", type);
		goto mount_failed;
	}

	// 给定数据一些缺省的值
	kernel_memset(fs, 0, sizeof(fs_t));
	kernel_strncpy(fs->mount_point, mount_point, FS_MOUNT_SIZE);
	fs->op = op;
	fs->mutex = (mutex_t *)0;

	// 挂载文件系统
	if (op->mount(fs, dev_major, dev_minor) < 0) {
		log_printf("mount fs %s failed", mount_point);
		goto mount_failed;
	}
	list_insert_last(&mounted_list, &fs->node);
	return fs;
mount_failed:
	if (fs) {
		// 回收fs
		list_insert_first(&free_list, &fs->node);
	}
	return (fs_t *)0;
}
// static fs_t * mount(fs_type_t type, char * mount_point, int dev_major, int dev_minor){
//     fs_t * fs = (fs_t *)0;
//     log_printf("mount file system,name: %s, dev: %x", mount_point, dev_major);
//     //先查看是否已经挂载了
//     list_node_t * curr = list_first(&mounted_list);
//     while(curr){
//         fs_t * fs = list_node_parent(curr, fs_t, node);
//         if(kernel_strncmp(fs->mount_point, mount_point, FS_MOUNT_SIZE) == 0){
//             log_printf("fs already ,ounted");
//             goto mount_failed;
//         }
//         curr = list_node_next(curr);
//     }
//     //从空闲链表中分配一个节点
//     list_node_t * free_node = list_remove_first(&free_list);
//     if(!free_node){
//         log_printf("no free fs, mount failed");
//         goto mount_failed;
//     }
//     //获取分配的节点的fs结构
//     fs = list_node_parent(free_node, fs_t, node);

//     //分配之后进行清零
//     kernel_memset(fs, 0, sizeof(fs_t));
//     //设置挂载点
//     kernel_strncpy(fs->mount_point, mount_point, FS_MOUNT_SIZE);
//     //根据文件系统类型获取文件系统操作结构
//     fs_op_t * op = fs->op = get_fs_op(type, dev_major);
//     if(!op){
//         log_printf("unsupported fs type: %d", type);
//         goto mount_failed;
//     }

//     fs->op = op;
//     //调用特定的文件系统的初始化
//     if(op->mount(fs, dev_major, dev_minor) < 0){
//         log_printf("mount failed");
//         goto mount_failed;
//     }
//     //挂载成功，加入挂载链表
//     list_insert_last(&mounted_list, &fs->node);
//     return fs;
// mount_failed:
//     if(fs){
//         list_insert_first(&free_list, &fs->node);
//     }

//     return (fs_t *)0;
// }

static void mount_list_init(void){
    list_init(&free_list);
    for(int i = 0; i < FS_TABLE_SIZE; i++){
        list_insert_first(&free_list, &fs_table[i].node);
    }
    list_init(&mounted_list);
}

void fs_init(void){
    mount_list_init();
    file_table_init();

    disk_init();

    fs_t * fs = mount(FS_DEVFS, "/dev", 0, 0);
    ASSERT(fs != (fs_t*)0);

    //根目录
//    root_fs = mount(FS_FAT16, "/home", ROOT_DEV);
//    ASSERT(root_fs != (fs_t*)0);
}


int path_to_num(const char * path, int * num){
    int n = 0;
    const char * c = path;
    while(*c){
        n = n * 10 + *c - '0';
        c++;
    }
    *num = n;
    return 0;
}
const char * path_next_child(const char * path){
    //          /dev/tty0  -> tty0
    const char *c = path;
    while(*c && (*c++ == '/')){}
    while(*c && (*c++ != '/')){}
    return *c ? c : (const char *)0;
}