#ifndef CPU_H
#define CPU_H

#include "comm/types.h"


#define EFLAGS_DEFAULT  (1 << 1)        //固定�?1的位
#define EFLAGS_IF       (1 << 9)  //中断使能位，设置�?1，开中断

//不能填充内存
#pragma pack(1)
typedef struct _segment_desc_t{
    uint16_t limit15_0;
    uint16_t base15_0;
    uint8_t base23_16;
    uint16_t attr;
    uint8_t base31_24;
}segment_desc_t;

//IDT表的表项——————中断门
typedef struct _date_desc_t{
    uint16_t offset15_0;
    uint16_t selector;
    uint16_t attr;
    uint16_t offset31_16;
}gate_desc_t;

#define GATE_TYPE_INT   (0xE << 8)
#define GATE_P_PRESENT (1 <<15)
#define GATE_DPL0   (0 << 13)
#define GATE_DPL3   (3 << 13)


typedef struct _tss_t{
    uint32_t pre_link;
    uint32_t esp0, ss0, esp1, ss1, esp2, ss2;    //不同特权级有相应的栈
    uint32_t cr3;   //任务，与虚拟页表有关�?
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;       //LDT�?,没有用到
    uint32_t iomap;     //IO权限位图，没有用�?
}tss_t;


#pragma pack()

#define SEG_G (1 << 15)     //G标志�???
#define SEG_D (1 << 14)     //为了兼容性支�???16位与32位，控制代码段是32位还�???16位，设置�???1�???32位代�???
// L与AVL与项目无�???
#define SEG_P_PRESENT   (1 << 7)    //     指示段描述符是否存在，存在为1
#define SEG_DPL0 (0 << 5)      //权限相关，操作系统运行在处理器上，可以给与较高的权限级别�???
                    //可以运行一些特殊的指令访问特殊的空间，如果是应用程序可以给低权限；
#define SEG_DPL3  (3 << 5)
#define SEG_S_SYSTEM    (0 << 4)       //是系统段还是普通的代码�???
#define SEG_S_NORMAL    (1 << 4)

#define SEG_TYPE_CODE   (1 << 3)
#define SEG_TYPE_DATA   (0 << 3)
#define SEG_TYPE_RW     (1 << 1)
#define SEG_TYPE_TSS    (9 << 0)

void segment_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr);
void cpu_init (void);
void gate_desc_set(gate_desc_t *desc, uint16_t selector, uint32_t offset, uint16_t attr);
int gdt_alloc_desc();
void switch_to_tss(int tss_sel);
void gdt_free_sel(int tss_sel);
#endif