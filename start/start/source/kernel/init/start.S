    
    /*启动文件不需要加.code16，gcc汇编器默认以32位汇编*/
    .text
    .extern kernel_init
    .global _start
_start:
    call kernel_init
    jmp .