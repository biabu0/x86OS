#ifndef MMU_H
#define MMU_H

#include "comm/types.h"
#include "comm/cpu_instr.h"

#define PDE_CNT     1024
#define PTE_P       (1 << 0)
#define PDE_P       (1 << 0)
#define PTE_W       (1 << 1)
#define PDE_W       (1 << 1)
#define PDE_U       (1 << 2)    //用户可以访问

//page directory entry 页目录表
typedef union _pde_t{
    uint32_t v;     //32位的表项
    struct {
        uint32_t present : 1;       //是否存在，分配4KB的内存页
        uint32_t write_enable : 1;   //写权限
        uint32_t user_mode_acc : 1;     //为1，用户模式可以访问
        uint32_t write_through : 1;
        uint32_t cache_disable : 1;
        uint32_t accessed : 1;
        uint32_t  : 1;
        uint32_t ps : 1;
        uint32_t :4;
        uint32_t phy_pt_addr : 20;      //存储页表的物理地址
    };
}pde_t;


//page table entry 页表
typedef union _pte_t{
    uint32_t v;     //32位的表项
    struct {
        uint32_t present : 1;       //是否存在，分配4KB的内存页
        uint32_t write_enable : 1;   //写权限
        uint32_t user_mode_acc : 1;     //为1，用户模式可以访问
        uint32_t write_through : 1;
        uint32_t cache_disable : 1;
        uint32_t accessed : 1;
        uint32_t dirty : 1;
        uint32_t pat : 1;
        uint32_t global : 1;
        uint32_t : 3;
        uint32_t phy_page_addr : 20;      //物理内存页地址
    };
}pte_t;


//将页目录地址写入cr3，出入页目录表物理地址
static inline void mmu_set_page_dir(uint32_t paddr){
    write_cr3(paddr);
}



static inline uint32_t pde_index(uint32_t vaddr){
    //将线性地址右移22为转换成页目录表项的索引
    return (vaddr >> 22);
}
static inline uint32_t pte_index(uint32_t vaddr){
    //将线性地址右移12位，将高10位清0，转换成页表项的索引
    return (vaddr >> 12) & 0x3ff;
}

//将页目录表项对应的页表地址返回
static inline uint32_t pde_paddr(pde_t * pde){
    return pde->phy_pt_addr << 12;
}
//将页表项对应的物理页地址返回
static inline uint32_t pte_paddr(pte_t * pte){
    return pte->phy_page_addr << 12;
}

#endif