/**
 * 自己动手写操作系统
 *
 * 系统引导部分，启动时由硬件加载运行，然后完成对二级引导程序loader的加载
 * 该部分程序存储于磁盘的第1个扇区，在计算机启动时将会由BIOS加载到0x7c00处
 * 之后，将由BIOS跳转至0x7c00处开始运行
 * 
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
	#include "boot.h"

  	// 16位代码，务必加上
  	.code16
	// 定义代码段表明之后的代码位于代码段,已经在CMakeLists中定义了此处为0x7c00的位置
 	.text
	.global _start
	.extern boot_entry
_start:
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	mov $_start, %esp
	
	mov $0xe, %ah
	mov $'w', %al
	int $0x10

	mov $0xe, %ah
	mov $'y', %al
	int $0x10

	mov $0xe, %ah
	mov $'l', %al
	int $0x10

	mov $0xe, %ah
	mov $'O', %al
	int $0x10

	mov $0xe, %ah
	mov $'S', %al
	int $0x10

read_loader:
	mov $0x8000, %bx	// 读取到的内存地址
	mov $0x2, %cx		// ch:磁道号,cl起始扇区号
	mov $0x2, %ah		// ah: 0x2读磁盘命令
	mov $64, %al		// al: 读取的扇区数量, 必须小于128,暂设置成32KB
	mov $0x0080, %dx	// dh: 磁头号,dl驱动器号0x80(磁盘1)
	int $0x13			//磁盘读取
	jc read_loader

	// 跳转至c部分执行, 再由c部分做一些处理
	jmp boot_entry

	jmp .
	
	// 引导结束段
	.section boot_end, "ax"
boot_sig: .byte 0x55, 0xaa
