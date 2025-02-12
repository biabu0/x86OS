#include "fs/fs.h"
#include "fs/file.h"
#include "comm/types.h"
#include "tools/klib.h"
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"

#include <sys/stat.h>
#include "dev/console.h"
#include "dev/dev.h"
#include "core/task.h"
#include "tools/log.h"

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

int sys_open(const char * name, int flags, ...){
    if(kernel_strncmp(name, "tty", 3) == 0){
        if(!is_path_vaild(name)){
            log_printf("path is not valid");
            return -1;
        }
        int fd = -1;
        file_t * file = file_alloc();
        if(file){
            fd = task_alloc_fd(file);
            if(fd < 0){
                goto sys_open_failed;
            }
        }else{
            goto sys_open_failed;
        }
        if(kernel_strlen(name) < 5){
            goto sys_open_failed;
        }
        int num = name[4] - '0';
        int dev_id = dev_open(DEV_TTY, num, 0);
        if(dev_id < 0){
            goto sys_open_failed;
        }
        file->dev_id = dev_id;
        file->mode = 0;
        file->pos = 0;
        file->ref = 1;
        kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);
        file->type = FILE_TTY;
        return fd;
sys_open_failed:
        if(file){
            file_free(file);
        }
        if(fd >= 0){
            task_remove_fd(fd);
        }
        return -1;
    }else{
        //如果是/则认为打开elf文件
        if(name[0] == '/'){
            read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
            temp_pos = TEMP_ADDR;
            return TEMP_FILE_ID;
        }
    }

    return -1;
}
int sys_read(int file, char * ptr, int len){
    if(TEMP_FILE_ID == file){
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    }else{
        file = 0;
        file_t * p_file = task_file(file);
        if(!p_file){
            log_printf("file not opened");
            return -1;
        }
        return dev_read(p_file->dev_id, 0, ptr, len);
    }

    return -1;
}

#include "tools/log.h"
int sys_write(int file, char * ptr, int len){
    // // 如果文件是标准输出，通过串口实现；希望printf函数的输出，能够显示到计算机的屏幕上，涉及显示器的处理
    // if(file == 1){
    //     ptr[len] = '\0';
    //     //console_write(0, ptr, len);     // 显示到屏幕上
    //     log_printf("%s",ptr);         // 显示到串口上
    // }

    file_t * p_file = task_file(file);
    if(!p_file){
        log_printf("file not opened");
        return -1;
    }
    return dev_write(p_file->dev_id, 0, ptr, len);
}
int sys_lseek(int file, int ptr, int dir){
    if(file == TEMP_FILE_ID){
        //简单地移动指针，偏移量为据开始的位置
        temp_pos = (uint8_t *)(TEMP_ADDR + ptr);
        return 0;
    }
    return -1;
} 
int sys_close(int file){
    return 0;
}

int sys_fstat(int file, struct stat *st){
    return -1;
}
int sys_isatty(int file){
    return -1;
}

int sys_dup(int file){
    if(file < 0 && file >= TASK_OFILE_NR){
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
        p_file->ref++;
        return fd;
    }
    log_printf("no task file avaliable");
    return -1;
}

void fs_init(void){
    file_table_init();
}
