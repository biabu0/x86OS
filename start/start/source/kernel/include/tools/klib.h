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

#endif