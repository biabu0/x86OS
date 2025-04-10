#include "dev/disk.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "dev/dev.h"
#include "cpu/irq.h"
#include "ipc/mutex.h"
#include "ipc/sem.h"

//  只考虑了主总线上面的两个磁盘，也就只需要一把锁一个信号量即可
static mutex_t mutex;
static sem_t op_sem;
static int task_on_op = 0;

//磁盘表，整个系统中所拥有的磁盘相关信息
static disk_t disk_buf[DISK_CNT];

static void disk_send_cmd(disk_t * disk, uint32_t start_sector, uint32_t sector_count, int cmd){
    outb(DISK_DRIVE(disk), DISK_DRIVE_BASE | disk->drive);		// 使用LBA寻址，并设置驱动器

	// 必须先写高字节
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count >> 8));	// 扇区数高8位
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 24));		// LBA参数的24~31位
	outb(DISK_LBA_MID(disk), 0);									// 高于32位不支持
	outb(DISK_LBA_HI(disk), 0);										// 高于32位不支持
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count));		// 扇区数量低8位
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 0));			// LBA参数的0-7
	outb(DISK_LBA_MID(disk), (uint8_t) (start_sector >> 8));		// LBA参数的8-15位
	outb(DISK_LBA_HI(disk), (uint8_t) (start_sector >> 16));		// LBA参数的16-23位

	// 选择对应的主-从磁盘
	outb(DISK_CMD(disk), (uint8_t)cmd);    
}

static void disk_read_data(disk_t * disk, void * buf, int size){
    uint16_t * c = (uint16_t *)buf;
    for(int i = 0; i < size / 2; i++){
        *c++ = inw(DISK_DATA(disk));
    }
}

static void disk_write_data(disk_t * disk, void * buf, int size){
    uint16_t * c = (uint16_t *)buf;
    for(int i = 0; i < size / 2; i++){
        outw(DISK_DATA(disk), *c++);
    }
}

static int disk_wait_data(disk_t * disk){
    uint8_t status;
	do {
        // 等待数据或者有错误
        status = inb(DISK_STATUS(disk));
        if ((status & (DISK_STATUS_BUSY | DISK_STATUS_DRQ | DISK_STATUS_ERR))
                        != DISK_STATUS_BUSY) {
            break;
        }
    }while (1);

    // 检查是否有错误
    return (status & DISK_STATUS_ERR) ? -1 : 0;
}


static void print_disk_info(disk_t * disk){
    log_printf("%s", disk->name);
    log_printf("   port base: %x", disk->port_base);
    log_printf("   total size: %d M", disk->sector_count * disk->sector_size / 1024 / 1024);
    for(int i = 0; i < DISK_PRIMARY_PART_CNT; i++){
        partinfo_t * part = disk->partinfo + i;
        if(part->type == FS_INVALID){
            continue;
        }else{
            log_printf("      %s: type: %x, start sector: %d, count: %d", 
            part->name, part->type, part->start_sector, part->total_sector);
        }
    }
}

//检测分区表信息，将其放置到disk->partinfo中
static int detect_part_info(disk_t * disk){
    mbr_t mbr;
    disk_send_cmd(disk, 0, 1, DISK_CMD_READ);
    int err = disk_wait_data(disk);
    if(err < 0){
        log_printf("read failed.");
        return err;
    }
    disk_read_data(disk, &mbr, sizeof(mbr));
    part_item_t * item = mbr.part_item;
    partinfo_t * part_info = disk->partinfo + 1;
    for(int i = 0; i < MBR_PRIMARY_PART_NR; i++, item++, part_info++){
        part_info->type = item->system_id;
        //分区表的类型不可用
        if(part_info->type == FS_INVALID){
            part_info->start_sector = 0;
            part_info->total_sector = 0;
            part_info->disk = (disk_t *)0;
        }else{
            kernel_sprintf(part_info->name, "%s%d", disk->name, i + 1);
            part_info->start_sector = item->start_sector;
            part_info->total_sector = item->total_sectors;
            part_info->disk = disk;
        }
    }
    return 0;

}

// 检测逻辑
static int identify_disk(disk_t * disk){    
    disk_send_cmd(disk, 0, 0, DISK_CMD_IDENTIFY);       //磁盘检测的前三步
    int err = inb(DISK_STATUS(disk));
    if(err == 0){
        log_printf("%s doesn't exist", disk->name);
        return -1;
    }
    err = disk_wait_data(disk);
    if(err < 0){
        log_printf("disk[%s] read failed.", disk->name);
        return err;
    }

    uint16_t buf[256];
    disk_read_data(disk, buf, sizeof(buf));
    disk->sector_count = *(uint32_t *)(buf + 100);
    disk->sector_size = SECTOR_SIZE;
    //设置第0个分区表项，描述整个磁盘
    partinfo_t * part = disk->partinfo + 0;
    part->disk = disk;
    kernel_sprintf(part->name, "%s%d",disk->name, 0);
    part->start_sector = 0;
    part->total_sector = disk->sector_count;
    part->type = FS_INVALID;
    // 检测分区信息
    detect_part_info(disk);
    return 0;
}



// 识别有多少硬盘，检测每块硬盘相应的特性放到disk_buf中
void disk_init(void){
    log_printf("Check disk...");
    kernel_memset(disk_buf, 0, sizeof(disk_buf));
    mutex_init(&mutex); 
    sem_init(&op_sem, 0);
    for(int i = 0; i < DISK_PER_CHANNL; i++){
        disk_t * disk = disk_buf + i;
        // 磁盘命名以sd 开头   sda, sdb , sd c , sdd
        kernel_sprintf(disk->name, "sd%c", 'a' + i);
        disk->drive = (i == 0)? DISK_MASTER : DISK_SLAVE;
        disk->port_base = IOBASE_PRIMARY;           //基地址

        // 赋值
        disk->mutex = &mutex;
        disk->op_sem = &op_sem;

        int err = identify_disk(disk);
        if(err == 0){
            print_disk_info(disk);
        }
    }

}
int disk_open(device_t *dev){
    // 0xa0 -- a 磁盘编号a,b,c...  0 分区号， 0,1,2...
    int disk_idx = (dev->minor >> 4) - 0xa;        //磁盘号
    int part_idx = dev->minor & 0xF;       //分区
    if((part_idx >= DISK_PRIMARY_PART_CNT) || (disk_idx >= DISK_CNT)){
        log_printf("disk_open: invalid minor %x", dev->minor);
        return -1;
    }
    //磁盘是否存在
    disk_t * disk = disk_buf + disk_idx;
    if(disk->sector_count == 0){
        log_printf("disk not exist, device : sd%x", dev->minor);
        return -1;
    }
    // 分区是否存在
    partinfo_t * part = disk->partinfo + part_idx;
    if(part->total_sector == 0){
        log_printf("part not exist, device : sd%x", dev->minor);
        return -1;
    }
    // 读取的时候需要知道分区信息，这里先将分区信息保存在dev的data中
    dev->data = part;
    irq_install(IRQ14_HARDDISK_PRIMARY, (irq_handler_t)exception_handler_ide_primary);
    irq_enable(IRQ14_HARDDISK_PRIMARY);
    return 0;
}

// addr是相对于分区开头的扇区数
int disk_read(device_t * dev, int addr, char * buf, int size){
    partinfo_t * part_info = (partinfo_t *)dev->data;
    if(!part_info){
        log_printf("disk_read: invalid device: %x", dev->minor);
        return -1;
    }
    disk_t * disk = part_info->disk;
    if(disk == (disk_t *)0){
        log_printf("No disk. device : %d", dev->minor);
        return -1;
    }
    mutex_locK(disk->mutex);
    task_on_op = 1;

    disk_send_cmd(disk, part_info->start_sector + addr, size, DISK_CMD_READ);
    int cnt;        //返回的扇区数，用于计算数据量
    for(cnt = 0; cnt < size; cnt++, buf+=disk->sector_size){
        //文件系统的挂载会在系统初始化的时候调用，这个时候操作系统还没有运行起来，需要对其判断，当前有进程运行则等待信号量
        if(task_current()){
            sem_wait(disk->op_sem);
        }
        
        int err = disk_wait_data(disk);
        if(err < 0){
            log_printf("disk(%s) read err: start sector %d, count: %d", disk->name, addr, size);
            break;
        }
        disk_read_data(disk, buf, disk->sector_size);
    }
    mutex_unlock(disk->mutex);
    return cnt;

}
int disk_write(device_t * dev, int addr, char * buf, int size){
    partinfo_t * part_info = (partinfo_t *)dev->data;
    if(!part_info){
        log_printf("disk_read: invalid device: %x", dev->minor);
    }
    disk_t * disk = part_info->disk;
    if(disk == (disk_t *)0){
        log_printf("No disk. device : %d", dev->minor);
    }
    mutex_locK(disk->mutex);
    task_on_op = 1;
    disk_send_cmd(disk, part_info->start_sector + addr, size, DISK_CMD_WRITE);
    int cnt;        //返回的扇区数，用于计算数据量
    for(cnt = 0; cnt < size; cnt++, buf+=disk->sector_size){
        disk_write_data(disk, buf, disk->sector_size);
        if(task_current()){
            sem_wait(disk->op_sem);
        }

        int err = disk_wait_data(disk);
        if(err < 0){
            log_printf("disk(%s) read err: start sector %d, count: %d", disk->name, addr, size);
            break;
        }
    }
    mutex_unlock(disk->mutex);
    return cnt;
}
    //设备特定的操作
int disk_control(device_t * dev, int cmd, int arg0, int arg1){
    return -1;
}
void disk_close(device_t * dev){
    return ;
}

void do_handler_ide_primary(exception_frame_t * frame){

    pic_send_eoi(IRQ14_HARDDISK_PRIMARY);       //发送中断结束信号
    if(task_on_op && task_current()){                //中断结束，表示已经处理完成，发送信号通知等待的进程
        sem_notify(&op_sem);
    }
}
dev_desc_t dev_disk_desc = {
    .name = "disk",
    .major = DEV_DISK,
    .open = disk_open,
    .read = disk_read,
    .write = disk_write,
    .control = disk_control,
    .close = disk_close
};