SECTIONS{
    /*将1M的地址处来时放.text*/
        /*  *是通配符, (gcc编译出来的源文件对应的目标文件，
    有其代码等数据)，* 将所有源文件对应的目标文件.text代码全部放到一个统一的.text中*/
    . = 0x10000;
    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)

    }

    .data : {
        *(.data)
    }
    
    .bss : {
        *(.bss)
    }

    PROVIDE(mem_free_start = .);
}