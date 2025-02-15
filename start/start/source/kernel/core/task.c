#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/mmu.h"
#include "comm/types.h"
#include "ipc/mutex.h"
#include "core/syscall.h"
#include "comm/elf.h"
#include "fs/fs.h"
static uint32_t idle_task_stack[IDLE_TASK_STACK_SIZE];
// 整个系统中只需要一个任务管理器，定义为全局变量
static task_manager_t task_manager;
//用于动态分配pid值
static mutex_t pid_mutex; 

static task_t task_table[TASK_NR];
static mutex_t task_table_mutex;


file_t * task_file(int fd){
    if((fd >= 0) && (fd < TASK_OFILE_NR)){
        file_t * file = task_current()->file_table[fd];
        return file;
    }
    return (file_t *)0;
}
int task_alloc_fd(file_t * file){
    task_t * task = task_current();
    for(int i = 0; i < TASK_OFILE_NR; i++){
        file_t * p = task->file_table[i];
        if(p == (file_t *) 0){
            task->file_table[i] = file;
            return i;
        }
    }
    return -1;
}
void task_remove_fd(int fd){
    if((fd >= 0) && (fd < TASK_OFILE_NR)){
        task_t * task = task_current();
        task->file_table[fd] = (file_t *)0;
    }
}

static int tss_init(task_t * task, int flag, uint32_t entry, uint32_t esp){
    int tss_sel = gdt_alloc_desc();
    if(tss_sel < 0){
        log_printf("alloc tss failed!\r\n");
        return -1;
    }
    //初始化GDT表项，segment_desc_set函数已经设置好将GDT表中的位置加载进去，也就是说现在GDT表中已经有了这个TSS描述符
    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t), 
        SEG_TYPE_TSS | SEG_DPL0 | SEG_P_PRESENT
    );

    kernel_memset(&task->tss, 0, sizeof(tss_t));    //清空tss
    uint32_t kernel_stack = memory_alloc_page();
    if(kernel_stack == 0){
        goto tss_init_failed;
    }
    int code_sel, data_sel;
    if(flag & TASK_FLAGS_SYSTEM){
        code_sel = KERNEL_SELECTOR_CS;
        data_sel = KERNEL_SELECTOR_DS;
    }else{
        code_sel = task_manager.app_code_sel | SEG_CPL3;
        data_sel = task_manager.app_data_sel | SEG_CPL3;
    }

    task->tss.eip = entry;      //CPU指令的位置，第一次运行时，将函数地址给eip保存起来
    task->tss.esp = esp;       //程序栈顶指针，将分配给进程的10页空间剩余的部分作为esp3的栈
    task->tss.esp0 = kernel_stack  + MEM_PAGE_SIZE; //esp0跟特权级有关，运行在特权级0, 单独分配栈空间，用于中断，系统调用等
    task->tss.ss = data_sel;      //平坦模型，数据段，也会用作栈空间
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.es = task->tss.ds = task->tss.fs = task->tss.gs = data_sel;
    task->tss.cs = code_sel;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;      //IF为1，我们不希望从TSS恢复后所有的中断不能响应
    //其他通用寄存器，设置为0，里面的值由程序自己设置
    //cr3，后序课程使用
    uint32_t page_dir = memory_create_uvm();
    if(page_dir == 0){
        goto tss_init_failed;
    }
    task->tss.cr3 = page_dir;//将页表设置到cr3
    task->tss_sel = tss_sel;        //将该任务的选择子保存起来，切换任务时，需要用到
    return 0;
tss_init_failed:
    //页表创建失败，释放描述符
    gdt_free_sel(tss_sel);
    if(kernel_stack){
        memory_free_page(kernel_stack);
    }
    return -1;
}


//分配pid值
static int allocate_pid(void){
    static int next_pid = 0;
    mutex_locK(&pid_mutex);
    next_pid++;
    mutex_unlock(&pid_mutex);
    return next_pid;
}

int task_init(task_t * task, const char * name, int flag, uint32_t entry, uint32_t esp){
    ASSERT(task != (task_t *)0);        //初始化，不能0
    tss_init(task, flag, entry, esp); //初始化了tss和tss_sel

    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->sleep_ticks = 0;
    task->heap_start = 0;
    task->heap_end = 0;

    task->time_ticks = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_ticks;

    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    kernel_memset(task->file_table, 0, sizeof(task->file_table));      //情况进程的文件描述符表

    irq_state_t state = irq_enter_protection();
    //直接将task结构地址作为pid，是唯一的
    task->pid = allocate_pid();
    //初始的时候父进程为0
    task->parent = (task_t *)0;
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);

    return 0;
}

void task_start(task_t * task){
    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    irq_leave_protection(state);
}

void task_uninit (task_t * task){
    //释放选择子
    if(task->tss_sel){
        gdt_free_sel(task->tss_sel);
    }
    //特权级0的栈
    if(task->tss.esp0){
        memory_free_page(task->tss.esp - MEM_PAGE_SIZE);
    }
    //销毁页表
    if(task->tss.cr3){
        memory_destroy_uvm(task->tss.cr3);
    }
    //将task结构体清空
    kernel_memset(task, 0, sizeof(task_t));

}

void simple_switch(uint32_t **from, uint32_t *to);
void task_switch_from_to (task_t *from, task_t *to){
    switch_to_tss(to->tss_sel);
    //simple_switch(&from->stack, to->stack);
};


// 初始化第一个进程，也就是任务管理器中的first_task
void task_first_init(void){
    void first_task_entry (void);
    extern uint8_t s_first_task[], e_first_task[];
    //程序数据，但不能只分配这么多，还要有栈等
    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;//分配10页的大小，first_task只是简单初始化一些程序，不会跑应用程序
    ASSERT(copy_size < alloc_size);

    uint32_t first_start = (uint32_t)first_task_entry;

    //first_task分配的10页大小的位置中其余部分作为栈空间,将其设置到特权级3的位置
    task_init(&task_manager.first_task, "first task", 0, first_start, first_start + alloc_size);

    //堆的起始地址就是在kernel.lds中定义的程序数据存储的结束地址，结束地址与起始地址是一样的；在first中没有预处理，C库也没有运行
    task_manager.first_task.heap_start = (uint32_t)e_first_task;
    task_manager.first_task.heap_end = (uint32_t)e_first_task;
    
    write_tr(task_manager.first_task.tss_sel);
    task_manager.cur_task = &task_manager.first_task;

    //在进行任务初始化的时候里面的tss初始化为该任务分配了一个页表放到了CR3中，使用该函数完成了页表的切换，此时，页表就是自己的页表
    //由于操作系统的代码依然在任务代页表中，并不会出现问题
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    //设置完页表后，给自己分配空间first_start为0x80000000，分配了10页，属性可写（将.data、.text等放在了一起）
    memory_alloc_page_for(first_start, alloc_size, PTE_P | PTE_W | PTE_U);
    //从物理地址开始copy到0x80000000，拷贝copy_size大小（实际大小）
    kernel_memcpy((void *)first_start, s_first_task, copy_size);

    task_start(&task_manager.first_task);

}
// 获取任务管理器中的第一个进程
task_t * task_first_task(void){
    return &task_manager.first_task;
}

static void idle_task_entry(void){
    for(;;){
        hlt();  //调用低功耗指令，让cpu处于低功耗状态，系统的开销就会少很多
    }
}
void task_mananger_init(void){
    kernel_memset(task_table, 0, sizeof(task_table));
    mutex_init(&task_table_mutex);

    //分配pid的时候的初始化mutex
    mutex_init(&pid_mutex);

    int sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_TYPE_DATA | SEG_S_NORMAL | SEG_D | SEG_TYPE_RW
    );
    task_manager.app_data_sel = sel;

    sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_TYPE_CODE | SEG_S_NORMAL | SEG_D | SEG_TYPE_RW
    );
    task_manager.app_code_sel = sel;
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    task_manager.cur_task = (task_t *)0;

    task_init(&task_manager.idle_task, 
        "idle task", 
        TASK_FLAGS_SYSTEM,
        (uint32_t)idle_task_entry, 
        (uint32_t)idle_task_stack + IDLE_TASK_STACK_SIZE
    );
    task_start(&task_manager.idle_task);
}

void task_set_ready(task_t *task){
    // 空闲进程不加入就绪队列中
    if(task == &task_manager.idle_task){
        return ;
    }

    list_insert_last(&task_manager.ready_list, &task->run_node);
    task->state = TASK_READY;
}

void task_set_block(task_t * task){
    if(task == &task_manager.idle_task){
        return ;
    }
    list_remove(&task_manager.ready_list, &task->run_node);
    //删除后不确定状态值，也就不需要设置
}


//处于整个队列头部的任务
task_t * task_next_run(void){
    if(list_count(&task_manager.ready_list) == 0){
        return &task_manager.idle_task;
    }
    list_node_t * node = list_first(&task_manager.ready_list);//获取就绪队列的第一个元素
    return list_node_parent(node, task_t, run_node);
}
task_t * task_current(void){
    return task_manager.cur_task;
}

int sys_sched_yield(void){
    //就绪队列中不止一个进程，将当前进程放到就绪队列的最后面
    irq_state_t state = irq_enter_protection();
    if(list_count(&task_manager.ready_list)>1){
        task_t * curr_task = task_current();    //取出当前任务
        task_set_block(curr_task);      //将当前进程从就绪队列中移除
        task_set_ready(curr_task);      //再放到就绪队列的最后面
        
        task_dispatch();
    }
    irq_leave_protection(state);
    return 0;
}


// 进行任务切换
void task_dispatch(void){
    irq_state_t state = irq_enter_protection();
    task_t * to = task_next_run();
    //要切换的任务与当前的任务不一样，切换
    if(to != task_manager.cur_task){
        task_manager.cur_task = to;
        to->state = TASK_RUNNING;

        task_switch_from_to(task_manager.cur_task, to);     //从前一个任务切换到下一个任务
    }
    irq_leave_protection(state);
}

void task_time_tick(void){
    task_t * curr_task = task_current();

    if(--curr_task->slice_ticks == 0){

        curr_task->slice_ticks = curr_task->time_ticks;      //重新设置时间片
        task_set_block(curr_task);      //将当前进程从就绪队列中移除
        task_set_ready(curr_task);      //再放到就绪队列的最后面

        task_dispatch();
    }

    // 定时中断检查睡眠队列中的任务是否到期
    list_node_t * curr = list_first(&task_manager.sleep_list);
    while(curr){
        list_node_t * next = curr->next;
        task_t * task = list_node_parent(curr, task_t, run_node);
        if(--task->sleep_ticks == 0){
            //从睡眠队列中移除
            task_set_wakeup(task);
            //加入到就绪队列中
            task_set_ready(task);
        }
        curr = next;
    }
    // 如果加入到就绪队列中需要立即运行
    task_dispatch();
}

// 在睡眠队列中待多少个时钟节拍
void task_set_sleep(task_t * task, uint32_t ticks){
    if(ticks == 0){
        return ;
    }
    task->state = TASK_SLEEP;
    task->sleep_ticks = ticks;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}
// 从睡眠队列中移除
void task_set_wakeup(task_t * task){
    list_remove(&task_manager.sleep_list, &task->run_node);
}

void sys_sleep(uint32_t ms){
    // 可能有多个进程加入到睡眠队列中，需要对睡眠队列进行保护
    irq_state_t state = irq_enter_protection();

    //将当前任务从就绪队列中移除
    task_set_block(task_manager.cur_task);

    // 将该任务加入到睡眠队列中，设置睡眠时间,向上取整
    task_set_sleep(task_manager.cur_task, (ms+(OS_TICKS_MS -1))/ OS_TICKS_MS);

    //将就绪队列中的当前任务切换到下一个任务
    task_dispatch();

    irq_leave_protection(state);
}

int sys_getpid(void){
    task_t *task = task_current();
    return task->pid;
}
//通过task结构体中的name是否为空来进行分配与释放
static task_t* alloc_task(void){
    task_t * task = (task_t *)0;
    mutex_locK(&task_table_mutex);
    for(int i = 0; i < TASK_NR; i++){
        task_t * curr = task_table + i;
        if(curr->name[0] == '\0'){
            task = curr;
            break;
        }
    }
    mutex_unlock(&task_table_mutex);
    return task;
}
static void free_task(task_t * task){
    mutex_locK(&task_table_mutex);
    task->name[0] = '\0';
    mutex_unlock(&task_table_mutex);
}

static void copy_opened_files(task_t * child_task){
    task_t * parent = child_task->parent;
    for(int i = 0; i < TASK_OFILE_NR; i++){
        file_t * file = parent->file_table[i];
        if(file){
            file_inc_ref(file);     //子进程关闭std三次,shell关闭std三次，共关闭6次，所以需要对file文件的打开次数增加
            child_task->file_table[i] = file;
        }
    }
    return ;
}

int sys_fork(void){
    task_t * parent_task = task_current();
    task_t * child_task = alloc_task();
    if(child_task == (task_t *)0){
        goto fork_failed;
    }
    //从父进程调用系统调用会自动压栈和手动压栈各个状态值存放在该结构体中，取出结构体起始地址，eip指向下一个指令的地址
    syscall_frame_t * frame = (syscall_frame_t *)(parent_task->tss.esp0 - sizeof(syscall_frame_t));
    //子进程没有做特权级的切换，就是在特权级3下面，只是保存tss和恢复tss，子进程与父进程运行位置一样，将父进程的esp给子进程,由于此时的父进程的
    //的esp是压入了参数的，故进行调整。
    int err = task_init(child_task, parent_task->name, 0, frame->eip, frame->esp + sizeof(uint32_t) * SYSCALL_PARAM_COUNT);
    if(err < 0){
        goto fork_failed;
    }
    
    //对子进程的tss进行初始化
    tss_t * tss = &child_task->tss;
    //将eax设置为0，这样子进程的返回值就是0
    tss->eax = 0;
    tss->ebx = frame->ebx;
    tss->ecx = frame->ecx;
    tss->edx = frame->edx;
    tss->esi = frame->esi;
    tss->edi = frame->edi;
    tss->ebp = frame->ebp;

    tss->cs = frame->cs;
    tss->ds = frame->ds;
    tss->es = frame->es;
    tss->fs = frame->fs;
    tss->gs = frame->gs;
    tss->eflags = frame->eflags;
    child_task->parent = parent_task;
    //单独为子进程分配页表
    if((child_task->tss.cr3 = memory_copy_uvm(parent_task->tss.cr3)) < 0){
        goto fork_failed;
    }
    copy_opened_files(child_task);
    task_start(child_task);
    
    return child_task->pid;
    //创建子进程失败
fork_failed:
    if(child_task){
        task_uninit(child_task);
        free_task(child_task);
    }
    return -1;
}


static int load_phdr(int file, Elf32_Phdr * phdr, uint32_t page_dir){
    //建立存储映射关系，将phdr映射进去，没有区分text还是data，全部设置为可写的
    int err = memory_alloc_for_page_dir(page_dir, phdr->p_vaddr, phdr->p_memsz, PTE_P | PTE_W | PTE_U);
    if(err < 0){
        log_printf("no memory");
        return -1;
    }
    //p_offset是程序头指向的段的偏移，p_vaddr是虚拟地址，可以读取程序数据了
    if(sys_lseek(file, phdr->p_offset, 0) < 0){
        log_printf("read file failed.");
        return -1;
    }
    //读取程序头指向的段的内容
    uint32_t vaddr = phdr->p_vaddr;
    uint32_t size = phdr->p_filesz;
    //目前的页表page_dir还没有启用，不能使用memory_copy，需要获取没有启用页表的物理地址，逐页拷贝
    while(size > 0){
        int curr_size = (size > MEM_PAGE_SIZE) ? MEM_PAGE_SIZE : size;
        //获取p_vaddr在page_dir中对应的物理地址
        uint32_t paddr = memory_get_paddr(page_dir, vaddr);
        //物理地址有对应的相同的虚拟地址
        if(sys_read(file, (char*)paddr, curr_size) < curr_size){
            log_printf("read file failed.");
            return -1;
        }
        size -= curr_size;
        vaddr += curr_size;
    }
    return 0;
}

static uint32_t load_elf_file(task_t *task, const char * pathname, uint32_t page_dir){
    Elf32_Ehdr elf_hdr;         //ELF头的结构，包含了ELF文件的信息
    Elf32_Phdr elf_phdr;        //程序头表项的内容
    int file = sys_open(pathname, 0);
    if(file < 0){
        log_printf("Open failed. %s", pathname);
        goto load_failed;
    }

    //将ELF头读入到elf_hdr中，保存了ELF文件的各种信息
    int cnt = sys_read(file, (char*)&elf_hdr, sizeof(elf_hdr));
    if(cnt < sizeof(Elf32_Ehdr)){
        log_printf("elf hdr too small. size = %d", cnt);
        goto load_failed;
    }
   // 做点必要性的检查。当然可以再做其它检查
   // 魔数检查0x7f E L F
    if ((elf_hdr.e_ident[0] != ELF_MAGIC) || (elf_hdr.e_ident[1] != 'E')
        || (elf_hdr.e_ident[2] != 'L') || (elf_hdr.e_ident[3] != 'F')) {
        log_printf("check elf indent failed.");
        goto load_failed;
    }
    // 必须是可执行文件和针对386处理器的类型，且有入口
    if ((elf_hdr.e_type != ET_EXEC) || (elf_hdr.e_machine != ET_386) || (elf_hdr.e_entry == 0)) {
        log_printf("check elf type or entry failed.");
        goto load_failed;
    }

    // 必须有程序头部
    if ((elf_hdr.e_phentsize == 0) || (elf_hdr.e_phoff == 0)) {
        log_printf("none programe header");
        goto load_failed;
    }

    //程序头表在ELF文件中的偏移
    uint32_t e_phoff = elf_hdr.e_phoff;
    for(int i = 0; i < elf_hdr.e_phnum; i++, e_phoff += elf_hdr.e_phentsize){
        //前面进行读的时候更改了读写指针，将其定义到程序表的开头
        //通过在文件中移动读写指针来读取不同的内容
        if(sys_lseek(file, e_phoff, 0) < 0){
            log_printf("lseek failed. read file failed.");
            goto load_failed;
        }
        //读取一个程序头表项,虚拟地址和物理地址都是0x81000000
        cnt = sys_read(file, (char*)&elf_phdr, sizeof(elf_phdr));
        if(cnt < sizeof(elf_phdr)){
            log_printf("read file failed.");
            goto load_failed;
        }

        //判断该程序头能否加载，虚拟内存地址位于0x80000000以下
        if((elf_phdr.p_type != 1) || (elf_phdr.p_vaddr < MEMORY_TASK_BASE)){
            continue;//不能加载则跳过，尝试加载下一个
        }

        //将程序头结构加载到内存中
        int err = load_phdr(file, &elf_phdr, page_dir);
        if(err < 0){
            log_printf("load program failed.");
            goto load_failed;
        }

        task->heap_start = elf_phdr.p_vaddr + elf_phdr.p_memsz;
        task->heap_end = task->heap_start;
    }
    sys_close(file);
    return elf_hdr.e_entry;
load_failed:
    if(file){
        sys_close(file);
    }
    return 0;
}

static int copy_args (char* to, uint32_t page_dir, int argc, char **argv){
    task_args_t task_args;
    task_args.argc = argc;
    task_args.argv = (char **)(to + sizeof(task_args_t));
    //返回地址目前不需要

    char * dest_arg = to + sizeof(task_args_t) + sizeof(char *) * argc;
    char ** dest_arg_tb = (char **)memory_get_paddr(page_dir, (uint32_t)(to + sizeof(task_args_t)));
    for(int i = 0; i < argc; i++){
        char * from = argv[i];
        //加上结束符
        int len = kernel_strlen(from) + 1;
        int err = memory_copy_uvm_data((uint32_t)dest_arg, page_dir, (uint32_t)from, len);
        ASSERT(err >= 0);
        dest_arg_tb[i] = dest_arg;
        dest_arg += len;

    }
    //将参数和环境变量拷贝到虚拟内存中
    return memory_copy_uvm_data((uint32_t)to, page_dir, (uint32_t)&task_args, sizeof(task_args));
}
int sys_execve(char * pathname, char * argv[], char * envp[]){
    task_t * task = task_current();

    //将应用名称拷贝到task中，用于显示进程名称
    kernel_strncpy(task->name, get_file_name(pathname), TASK_NAME_SIZE);
    uint32_t old_page_dir = task->tss.cr3;
    //为新的进程重新分配一个新的页表，不再使用之前的页表
    uint32_t new_page_dir = memory_create_uvm();
    if(!new_page_dir){
        goto exec_failed;
    }

    //扫描elf文件的表头，将相应的数据拷贝到虚拟内存中
    uint32_t entry = load_elf_file(task, pathname, new_page_dir);
    if(entry == 0){
        goto exec_failed;
    }

    //定义栈空间的大小和位置，将其与页表建立映射
    //MEM_TASK_ARG_SIZE预留空间放置参数和环境变量
    uint32_t stack_top = MEM_TASK_STACK_TOP - MEM_TASK_ARG_SIZE;
    int err = memory_alloc_for_page_dir(
        new_page_dir, MEM_TASK_STACK_TOP - MEM_TASK_STACK_SIZE,
        MEM_TASK_STACK_SIZE, PTE_P | PTE_W | PTE_U
    );
    if(err < 0){
        goto exec_failed;
    }
    int argc = string_count(argv);
    err = copy_args((char *)stack_top, new_page_dir, argc, argv);
    if(err < 0){
        goto exec_failed;
    }

    syscall_frame_t * frame = (syscall_frame_t *)(task->tss.esp0 - sizeof(syscall_frame_t));
    //eip修改为ELF的入口地址
    frame->eip = entry;
    frame->eax = frame->ebx = frame->ecx = frame->edx = frame->ebp = frame->edi = frame->esi = 0;
    frame->eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    //所有的应用程序的段寄存器是同一个，不需要修改
    //将栈设置好，要减去参数的空间
    frame->esp = stack_top - sizeof(uint32_t) * SYSCALL_PARAM_COUNT;


    //更新新的页表，并不总是能够立马生效
    task->tss.cr3 = new_page_dir;
    //强制更新，将new_page_dir更新到cr3中
    mmu_set_page_dir(new_page_dir);
    //销毁原来的页表(0x80000000)
    memory_destroy_uvm(old_page_dir);

    return 0;

exec_failed:
    if(new_page_dir){
        task->tss.cr3 = old_page_dir;
        mmu_set_page_dir(new_page_dir);

        memory_destroy_uvm(new_page_dir);
    }
    //释放页表
    return -1;
}

int sys_wait(int* status) {
    task_t * curr_task = task_current();
    for (;;) {
        // 遍历，找僵尸状态的进程，然后回收。如果收不到，则进入睡眠态
        mutex_locK(&task_table_mutex);
        for (int i = 0; i < TASK_NR; i++) {
            task_t * task = task_table + i;
            if (task->parent != curr_task) {
                continue;
            }

            if (task->state == TASK_ZOMBIE) {
                int pid = task->pid;

                *status = task->status;

                memory_destroy_uvm(task->tss.cr3);          //释放应用空间
                memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);            //释放任务栈空间
                kernel_memset(task, 0, sizeof(task_t));

                mutex_unlock(&task_table_mutex);
                return pid;
            }
        }
        mutex_unlock(&task_table_mutex);

        // 找不到，则等待
        irq_state_t state = irq_enter_protection();
        task_set_block(curr_task);
        curr_task->state = TASK_WAITTING;
        task_dispatch();
        irq_leave_protection(state);
    }
}

/**
 * @brief 退出进程
 */
void sys_exit(int status) {
    task_t * curr_task = task_current();

    // 关闭所有已经打开的文件, 标准输入输出库会由newlib自行关闭，但这里仍然再处理下
    for (int fd = 0; fd < TASK_OFILE_NR; fd++) {
        file_t * file = curr_task->file_table[fd];
        if (file) {
            sys_close(fd);
            curr_task->file_table[fd] = (file_t *)0;
        }
    }

    int move_child = 0;     //是否子进程处于僵死状态

    // 找所有的子进程，将其转交给init进程
    mutex_locK(&task_table_mutex);
    for (int i = 0; i < TASK_OFILE_NR; i++) {
        task_t * task = task_table + i;
        // 如果要exit的进程有子进程，则需要将子进程转交给init进程
        if (task->parent == curr_task) {
            // 有子进程，则转给init_task
            task->parent = &task_manager.first_task;

            // 如果子进程中有僵尸进程，唤醒回收资源
            // 并不由自己回收，因为自己将要退出
            if (task->state == TASK_ZOMBIE) {
                move_child = 1;
            }
        }
    }
    mutex_unlock(&task_table_mutex);


    irq_state_t state = irq_enter_protection();

    // 如果有移动子进程，则唤醒init进程
    task_t * parent = curr_task->parent;
    if (move_child && (parent != &task_manager.first_task)) {  // 如果父进程为init进程，在下方唤醒
        if (task_manager.first_task.state == TASK_WAITTING) {
            task_set_ready(&task_manager.first_task);
        }
    }

    // 如果有父任务在wait，则唤醒父任务进行回收
    // 如果父进程没有等待，则一直处理僵死状态？
    if (parent->state == TASK_WAITTING) {
        task_set_ready(curr_task->parent);
    }

    // 保存返回值，进入僵尸状态
    curr_task->status = status;
    curr_task->state = TASK_ZOMBIE;
    task_set_block(curr_task);
    task_dispatch();

    irq_leave_protection(state);
}