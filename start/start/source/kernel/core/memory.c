#include "core/memory.h"

#include "tools/klib.h"
#include "tools/log.h"
#include "cpu/mmu.h"

static addr_alloc_t paddr_alloc;

//定义页目录表 大小为1024，将该地址给CR3寄存器，对齐，CR3对低12位无效
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE)));

static void addr_alloc_init(addr_alloc_t * alloc, uint8_t *bits, 
    uint32_t start, uint32_t size, uint32_t page_size){

    mutex_init(&alloc->mutex);
    alloc->start = start;
    alloc->size = size;
    alloc->page_size = page_size;
    bitmap_init(&alloc->bitmap, bits, alloc->size/page_size, 0);
}

//分配count个页，使用互斥锁，确保原子操作
static uint32_t addr_alloc_page(addr_alloc_t * alloc, int page_count){
    mutex_locK(&alloc->mutex);
    uint32_t addr = 0;
    int page_index = bitmap_alloc_nbits(&alloc->bitmap, 0, page_count);
    if(page_index >= 0){
        addr = alloc->start + page_index * alloc->page_size;        //得到分配的起始地址
    }
    mutex_unlock(&alloc->mutex);
    return addr;
}

static void addr_free_page(addr_alloc_t * alloc, uint32_t addr, int page_count){
    mutex_locK(&alloc->mutex);
    uint32_t pg_index = (addr - alloc->start) / alloc->page_size;
    bitmap_set_bit(&alloc->bitmap, pg_index, page_count, 0);//释放，设置为0
    mutex_unlock(&alloc->mutex);
}

void show_mem_info(boot_info_t * boot_info){
    log_printf("mem region: ");
    for(int i = 0; i < boot_info->ram_region_count; i++){
        log_printf("[%d] 0x%x - 0x%x", i, boot_info->ram_region_cfg[i].start, boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

static uint32_t total_mem_size(boot_info_t * boot_info){
    uint32_t mem_size  = 0;
    for(int i = 0; i < boot_info->ram_region_count; i++){
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}

pte_t * find_pte(pde_t * page_dir, uint32_t vaddr, int alloc){
    pte_t * page_table;
    pde_t * pde =  page_dir + pde_index(vaddr);
    if(pde->present){   //对应的表项有效，即该页目录表的该表项有对应的页表，获取页表的地址
        page_table = (pte_t *)pde_paddr(pde);
    }else{
        if(alloc == 0){//不存在对应的表项，且不分配
            return (pte_t *)0;
        }
        uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);       //分配页表项4KB大小也就1个页//第一次分配会分配到1M的位置
        if(pg_paddr == 0){         //分配失败
            return (pte_t *)0;
        }
        pde->v = pg_paddr | PDE_P | PDE_W | PDE_U;
        page_table = (pte_t *)pg_paddr;//分配的表项如果内部有数据，则需要清空
        kernel_memset(page_table, 0, MEM_PAGE_SIZE);
    }

    return page_table + pte_index(vaddr);//获得pte的表项
}

int mem_create_map(pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int page_count, uint32_t perm){
    for(int i = 0; i < page_count; i++){
        //log_printf("create map: v-0x%x, p-0x%x, perm:0x%x", vaddr, paddr, perm);
        pte_t * pte = find_pte(page_dir, vaddr, 1);//找到页表项，为1 ，没有页表项则分配页表项，这里获得的是页表中的一个表项
        if(pte == (pte_t *)0){      //没有找到
            //log_printf("create map failed. pte == 0");
            return -1;
        }
        //log_printf("pte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);
        pte->v = paddr | perm | PTE_P;
        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }
}

void create_kernel_table(void){
    extern uint8_t s_text[], e_text[], s_data[];
    extern uint8_t kernel_base[];
    static memory_map_t kernel_map[] = {
        //0地址到代码段时可读写的数据区
        {kernel_base, s_text, kernel_base, PTE_W}, 
        // 程序和只读数据，只读
        {s_text, e_text, s_text, 0},
        // 可读写的数据空间
        {s_data, (void*)MEM_EBDA_START, s_data, PTE_W},
        // 将1M到128M的区域映射到物理内存中
        {(void *)MEM_EXT_START, (void *)MEM_EXT_END, (void *)MEM_EXT_START, PTE_W}
    };
    for(int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); i++){
        memory_map_t * map = kernel_map + i;
        
        uint32_t vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);//将其对齐到页大小
        uint32_t vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        uint32_t paddr = down2((uint32_t)map->pstart, MEM_PAGE_SIZE);
        int page_count = (vend - vstart)  / MEM_PAGE_SIZE;

        // 将虚拟地址映射到物理地址,传入映射多少个页，设置的属性
        // 进程中可能有多个页表，每个进程可能有分页机制的页表，方便起见，传入要设置的页表
        mem_create_map(kernel_page_dir, vstart, (uint32_t)paddr, page_count, map->perm);
    }
}

//创建用户的页表
uint32_t memory_create_uvm(void){
    //分配一页内存
    pde_t * page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if(page_dir == 0){
        return 0;
    }
    //清空页表
    kernel_memset((void*)page_dir, 0, MEM_PAGE_SIZE);
    //将0x80000000以下的虚拟内存映射
    uint32_t user_pde_start = pde_index(MEMORY_TASK_START);
    for(int i = 0; i < user_pde_start; i++){
        page_dir[i].v = kernel_page_dir[i].v;
    }
    return (uint32_t)page_dir;
}
void memory_init(boot_info_t * boot_info){
    extern uint8_t * mem_free_start;    //外部声明
    //log_printf("mem init");
    show_mem_info(boot_info);//显示内存信息

    uint8_t * mem_free = (uint8_t *)&mem_free_start;//mem_free_start无法修改，使用额外的变量
    //mem_up1MB_free应该是页大小（如4kb）的整数倍
    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;//MEM_EXT_START为1MB
    mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);//转换成4KB大小的整数倍向下取整
    //log_printf("free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);//其实地址和大小

    addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, mem_up1MB_free, MEM_PAGE_SIZE);

    mem_free += bitmap_byte_count(paddr_alloc.size/MEM_PAGE_SIZE);//将地址向后移动，跳过位图分配的区域
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    create_kernel_table();      //创建内核页表
    mmu_set_page_dir((uint32_t)kernel_page_dir);    //将页目录表地址赋值给CR3寄存器，开启分页机制
}