#ifndef DISK_H
#define DISK_H

#include "comm/types.h"
#include "ipc/mutex.h"
#include "ipc/sem.h"

#define PART_NAME_SIZE          32
#define DISK_NAME_SIZE          32
#define DISK_PRIMARY_PART_CNT   (4+1)
#define DISK_CNT                2
#define DISK_PER_CHANNL         2//每个通道最多支持两个磁盘

// https://wiki.osdev.org/ATA_PIO_Mode#IDENTIFY_command
// 只考虑支持主总结primary bus
#define IOBASE_PRIMARY  0x1F0       //io寄存器基地址，次总线不再设置
#define	DISK_DATA(disk)				(disk->port_base + 0)		// 数据寄存器
#define	DISK_ERROR(disk)			(disk->port_base + 1)		// 错误寄存器
#define	DISK_SECTOR_COUNT(disk)		(disk->port_base + 2)		// 扇区数量寄存器
#define	DISK_LBA_LO(disk)			(disk->port_base + 3)		// LBA寄存器
#define	DISK_LBA_MID(disk)			(disk->port_base + 4)		// LBA寄存器
#define	DISK_LBA_HI(disk)			(disk->port_base + 5)		// LBA寄存器
#define	DISK_DRIVE(disk)			(disk->port_base + 6)		// 磁盘或磁头？
#define	DISK_STATUS(disk)			(disk->port_base + 7)		// 状态寄存器
#define	DISK_CMD(disk)				(disk->port_base + 7)		// 命令寄存器


// ATA命令
#define	DISK_CMD_IDENTIFY				0xEC	// IDENTIFY命令
#define	DISK_CMD_READ					0x24	// 读命令
#define	DISK_CMD_WRITE					0x34	// 写命令

// 状态寄存器
#define DISK_STATUS_ERR          (1 << 0)    // 发生了错误
#define DISK_STATUS_DRQ          (1 << 3)    // 准备好接受数据或者输出数据
#define DISK_STATUS_DF           (1 << 5)    // 驱动错误
#define DISK_STATUS_BUSY         (1 << 7)    // 正忙

#define DISK_DRIVE_BASE         0xE0 //对应0x1F6寄存器后面5-7为1

#define MBR_PRIMARY_PART_NR	    4   // 4个分区表
#pragma pack(1)

/**
 * MBR的分区表项类型
 */
typedef struct _part_item_t {
    uint8_t boot_active;               // 分区是否活动
	uint8_t start_header;              // 起始header
	uint16_t start_sector : 6;         // 起始扇区
	uint16_t start_cylinder : 10;	    // 起始磁道
	uint8_t system_id;	                // 文件系统类型
	uint8_t end_header;                // 结束header
	uint16_t end_sector : 6;           // 结束扇区
	uint16_t end_cylinder : 10;        // 结束磁道
	uint32_t relative_sectors;	        // 相对于该驱动器开始的相对扇区数   LBA模式下指定的扇区号
	uint32_t total_sectors;            // 总的扇区数        
}part_item_t;



/**
 * MBR区域描述结构
 */
typedef  struct _mbr_t {
	uint8_t code[446];                 // 引导代码区
    part_item_t part_item[MBR_PRIMARY_PART_NR];     // 分区表
	uint8_t boot_sig[2];               // 引导标志
}mbr_t;

#pragma pack()


struct _disk_t;
//描述某个磁盘的某个分区
typedef struct _partinfo_t{
    char name[PART_NAME_SIZE];
    struct _disk_t * disk;      //该分区所在的磁盘
    // 该分区的文件系统类型
    enum{
        FS_INVALID = 0x00,
        FS_FAT16_0 = 0x6,
        FS_FAT16_1 = 0xE,
    }type;
    int start_sector;
    int total_sector;
}partinfo_t;


//定义一个磁盘结构，描述特定的磁盘
typedef struct _disk_t{
    char name[DISK_NAME_SIZE];

    enum{
        DISK_MASTER = (0 << 4),        //主磁盘
        DISK_SLAVE =  (1 << 4),     //从磁盘
    }drive;

    uint16_t port_base;     //IO端口地址

    int sector_size;        //扇区大小
    int sector_count;       //扇区总数
    partinfo_t partinfo[DISK_PRIMARY_PART_CNT];// 分区表, 包含一个描述整个磁盘的假分区信息

    mutex_t * mutex;
    sem_t * op_sem;
}disk_t;

void disk_init(void);
void exception_handler_ide_primary(void);
#endif