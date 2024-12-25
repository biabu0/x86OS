#include "core/memory.h"

#include "tools/klib.h"
#include "tools/log.h"


static addr_alloc_t paddr_alloc;
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
void memory_init(boot_info_t * boot_info){
    extern uint8_t * mem_free_start;    //外部声明
    log_printf("mem init");
    show_mem_info(boot_info);//显示内存信息

    uint8_t * mem_free = (uint8_t *)&mem_free_start;//mem_free_start无法修改，使用额外的变量
    //mem_up1MB_free应该是页大小（如4kb）的整数倍
    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;//MEM_EXT_START为1MB
    mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);//转换成4KB大小的整数倍向下取整
    log_printf("free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);//其实地址和大小

    addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, mem_up1MB_free, MEM_PAGE_SIZE);

    mem_free += bitmap_byte_count(paddr_alloc.size/MEM_PAGE_SIZE);//将地址向后移动，跳过位图分配的区域
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

}