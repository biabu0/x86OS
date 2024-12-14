#ifndef KLIB_H
#define KLIB_H

#include "comm/types.h"
#include <stdarg.h>

void kernel_strcpy(char * dest, const char * str);
void kernel_strncpy(char * dest, const char * str, int size);
int kernel_strncmp (const char *s1, const char * s2, int size);
    /*
        return 0    相等
            -1      不等
    */
int kernel_strlen(const char * str);
/*
    返回有效字符，不包含尾零。
*/
void kernel_memcpy(void * dest, void * src, int size);
void kernel_memset(void * dest, uint8_t v, int size);
int kernel_memcmp(void * d1, void * d2, int size);

void kernel_sprintf(char * buf, const char *fmt, ...);
void kernel_vsprintf(char * buf, const char *fmt, va_list args);

//没有定义RELEASE，说明在调试过程中
#ifndef RELEASE
#define ASSERT(expr)    \
    if (!(expr)) panic(__FILE__, __LINE__, __func__, #expr)
        //编译器内置的宏，编译的时候会替换

//传入哪个文件，哪一行，哪个函数，出现什么问题
void panic(const char * file, int line, const char * func, const char * cond);
#else
#define ASSERT(expr)   ((void)0)
#endif

#endif