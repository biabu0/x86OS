#ifndef BITMAP_H
#define BIT_MAP_H

#include "comm/types.h"

typedef struct _bitmap_t{
    int bit_count;      //位数，能支持的最大空间
    uint8_t * bits;//字节数组
}bitmap_t;
int bitmap_byte_count(int bit_count);
void bitmap_init(bitmap_t * bitmap, uint8_t * bits, int count, int init_bit);
//获取位
int bitmap_get_bit (bitmap_t * bitmap, int index);
//设置
void bitmap_set_bit (bitmap_t * bitmap, int index, int count, int bit);
//判断是否设置成功
int bitmap_is_set (bitmap_t* bitmap, int index);
//分配多个页
int bitmap_alloc_nbits(bitmap_t* bitmap, int bit, int count);
#endif