SECTIONS{
    /*将1M的地址处来时放.text*/
        /*  *是通配符, (gcc编译出来的源文件对应的目标文件，
    有其代码等数据)，* 将所有源文件对应的目标文件.text代码全部放到一个统一的.text中*/
    PROVIDE(kernel_base = .);
    . = 0x10000;

    PROVIDE(s_text = .);
    .text : {
        *(EXCLUDE_FILE(*first_task*) .text)
    }

    .rodata : {
        *(EXCLUDE_FILE(*first_task*) .rodata)

    }
    PROVIDE(e_text = .);
    . = ALIGN(4096);
    PROVIDE(s_data = .);
    .data : {
        *(EXCLUDE_FILE(*first_task*) .data)
    }
    
    .bss : {
        *(EXCLUDE_FILE(*first_task*) .bss)
    }

    e_data = .;

    . = 0x80000000;
    PROVIDE(s_first_task = LOADADDR(.first_task));
    .first_task : AT(e_data){
    *first_task_entry*(.text .rodata .bss .data)
    *first_task*(.text .rodata .bss .data)
    }

    PROVIDE(e_first_task = LOADADDR(.first_task) + SIZEOF(.first_task));
    PROVIDE(mem_free_start = e_first_task);
}