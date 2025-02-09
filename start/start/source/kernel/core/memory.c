#include "core/memory.h"

#include "tools/klib.h"
#include "tools/log.h"
#include "cpu/mmu.h"
#include "dev/console.h"

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
        // 将显存地址映射，可写就行
        {(void *)CONSOLE_DISP_ADDR, (void *)CONSOLE_DISP_END, (void *)CONSOLE_DISP_ADDR, PTE_W},
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

//创建用户的第一级页表，并进行初始化
uint32_t memory_create_uvm(void){
    //分配一页内存
    pde_t * page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if(page_dir == 0){
        return 0;
    }
    //清空页表
    kernel_memset((void*)page_dir, 0, MEM_PAGE_SIZE);
    //将0x80000000以下的虚拟内存映射，这些是操作系统的内容，个进程都是一样的，以后只需要映射以上的即可
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
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
int memory_alloc_for_page_dir(uint32_t page_dir, uint32_t vaddr, uint32_t size, int perm){
    uint32_t curr_vaddr = vaddr;
    int page_count = up2(size, MEM_PAGE_SIZE) / MEM_PAGE_SIZE;
    
    for(int i = 0; i < page_count; i++){
        uint32_t paddr = addr_alloc_page(&paddr_alloc, 1);
        if(paddr == 0){
            log_printf("mem alloc failed . ne memory");
            return -1;
        }

        int err = mem_create_map((pde_t *)page_dir, curr_vaddr, paddr, 1, perm);
        if(err < 0){
            log_printf("create memory failed. err = %d", err);
            return -1;
        }

        curr_vaddr += MEM_PAGE_SIZE;
    }
    return 0;
}
int memory_alloc_page_for(uint32_t addr, uint32_t size, int perm){
    //该函数会指定具体给那个页表分配空技能
    return memory_alloc_for_page_dir(task_current()->tss.cr3, addr, size, perm);
}

uint32_t memory_alloc_page(void){
    //返回的物理地址，但之前在create_kernel_table中已经将1M以上的地址与线性地址映射了
    uint32_t addr = addr_alloc_page(&paddr_alloc, 1);
    return addr;
}
static pde_t *curr_page_dir(void){
    return (pde_t *)(task_current()->tss.cr3);
}
void memory_free_page(uint32_t addr){
    //如果分配的地址是0x80000000以内，则说明使用的是memory_alloc_page分配的地址，不然是memory_alloc_page_for分配的
    //对应的是0x80000000以上的虚拟地址
    if(addr < MEMORY_TASK_START){
        addr_free_page(&paddr_alloc, addr, 1);//物理地址
    }else{
        pte_t * pte = find_pte(curr_page_dir(), addr, 0);
        ASSERT((pte == (pte_t *)0) && pte->present);
        addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
        pte->v = 0;//解除映射关系
    }
}

void memory_destroy_uvm(uint32_t page_dir){
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;
    for(int i = user_pde_start; i < PDE_CNT; i++, pde++){
        //如果该一级页表的索引不存在，则跳过，即无需拷贝
        if(!pde->present){
            continue;
        }
        //如果存在，则需要进行复制，取出物理页地址
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for(int j = 0; j < PTE_CNT; j++, pte++){
            if(!pte->present){
                continue;
            }
            addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
        }

        addr_free_page(&paddr_alloc, (uint32_t)pde_paddr(pde), 1);
    }

    addr_free_page(&paddr_alloc, (uint32_t)page_dir, 1);
}

//给子进程复制父进程页表
uint32_t memory_copy_uvm(uint32_t page_dir){
    //创建一个一级页表
    uint32_t to_page_dir = memory_create_uvm();
    //创建失败
    if(to_page_dir == 0){
        goto copy_uvm_failed;
    }
    //0x80000000对应的一级页表的索引
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;
    for(int i = user_pde_start; i < PDE_CNT; i++, pde++){
        //如果该一级页表的索引不存在，则跳过，即无需拷贝
        if(!pde->present){
            continue;
        }
        //如果存在，则需要进行复制，取出物理页地址
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for(int j = 0; j < PTE_CNT; j++, pte++){
            if(!pte->present){
                continue;
            }
            //分配的物理页的地址
            uint32_t page = addr_alloc_page(&paddr_alloc, 1);
            if(page == 0){
                goto copy_uvm_failed;
            }
            //线性地址分为三部分，一级页表、二级页表和偏移，将父进程的映射关系拷贝到子进程
            uint32_t vaddr = (i << 22) | (j << 12);
            int err = mem_create_map((pde_t *) to_page_dir, vaddr, page, 1, get_pte_perm(pte));
            if(err < 0){
                goto copy_uvm_failed;
            }
            //将父进程的物理页复制到子进程
            kernel_memcpy((void*)page, (void *)vaddr, MEM_PAGE_SIZE);
        }

    }
    return to_page_dir;
copy_uvm_failed:
    if(to_page_dir){
        memory_destroy_uvm(to_page_dir);
    }
    return -1;
}

uint32_t memory_get_paddr(uint32_t page_dir, uint32_t vaddr){
    pte_t * pte = find_pte((pde_t *)page_dir, vaddr, 0);
    if(!pte){
        return 0;
    }
    //vaddr 0x1024 & 0xFFF = 0x024偏移量
    //1000->0xFFF
    return pte_paddr(pte) + (vaddr & (MEM_PAGE_SIZE - 1));
}

int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size){
    while(size > 0){
        //物理地址与虚拟地址是对应的，from是当前已经启用的页表，看到的虚拟地址是连续的，而page_dir还没有启用，
        //通过物理地址进行copy（物理地址会和虚拟第一一对应，是一样的）
        uint32_t to_paddr = memory_get_paddr(page_dir, to);
        if(to_paddr == 0){
            return -1;
        }
        //to对应的物理地址可能是不连续的需要逐页copy
        uint32_t offset_in_page = to_paddr & (MEM_PAGE_SIZE -1);
        uint32_t curr_size = MEM_PAGE_SIZE - offset_in_page;
        if(curr_size > size){
            curr_size = size;
        }
        kernel_memcpy((void *)to_paddr, (void *)from, curr_size);

        size -= curr_size;
        to += curr_size;
        from += curr_size;
    }

    return 0;
}


char * sys_sbrk(int incr){
    task_t * task = task_current();
    char * pre_head_end = (char *)task->heap_end;
    int pre_incr = incr;
    ASSERT(incr >= 0);
    if(incr == 0){
        log_printf("sbrk(0): end = 0x%x", pre_head_end);
        return pre_head_end;
    }
    uint32_t start = task->heap_end;
    uint32_t end = start + incr;

    //如果start不是页边界对齐的 0X81001024
    int start_offset = start % MEM_PAGE_SIZE;    //0X24

    //起始地址不是页边界对齐的
    if(start_offset){
        //如果偏移量与要分配的内存没有超过一页的大小，则直接在原页内进行分配
        if(start_offset + incr <= MEM_PAGE_SIZE){
            task->heap_end = end;
            log_printf("sbrk(%d): end = 0x%x", incr, end);
            return pre_head_end;
        }else{
            //当前页中还没有分配的内存
            uint32_t curr_size = MEM_PAGE_SIZE - start_offset;
            start += curr_size;
            incr -= curr_size;
        }
    }
    if(incr){
        uint32_t curr_size = end - start;
        int err = memory_alloc_page_for(start, curr_size, PTE_P | PTE_W | PTE_U);
        if(err < 0){
            log_printf("sbrk: alloc mem failed.");
            return (char *)-1;
        }
    }
    log_printf("sbrk(%d): end = 0x%x", pre_incr, end);
    task->heap_end = end;
    return pre_head_end;
}