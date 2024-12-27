SECTIONS{
    /*将1M的地址处来时放.text*/
        /*  *是通配符, (gcc编译出来的源文件对应的目标文件，
    有其代码等数据)，* 将所有源文件对应的目标文件.text代码全部放到一个统一的.text中*/
    PROVIDE(kernel_base = .);
    . = 0x10000;

    PROVIDE(s_text = .);
    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)

    }
    PROVIDE(e_text = .);
    . = ALIGN(4096);
    PROVIDE(s_data = .);
    .data : {
        *(.data)
    }
    
    .bss : {
        *(.bss)
    }

    PROVIDE(mem_free_start = .);
}