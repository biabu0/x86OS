#include "tools/bitmap.h"
#include "tools/klib.h"

int bitmap_byte_count(int bit_count){
    return (bit_count + 8 - 1) / 8;
}

void bitmap_init(bitmap_t * bitmap, uint8_t * bits, int count, int init_bit){
    bitmap->bit_count = count;
    bitmap->bits = bits;

    int bytes = bitmap_byte_count(bitmap->bit_count);
    kernel_memset(bitmap->bits, init_bit ? 0xFF : 0x00, bytes);
}

//从一个位图（bitmap）中获取指定索引（index）的位（bit）的值
int bitmap_get_bit (bitmap_t * bitmap, int index){
    return bitmap->bits[index / 8] & (1 << (index % 8));
}
//设置
void bitmap_set_bit (bitmap_t * bitmap, int index, int count, int bit){
    /*
        bitmap_t * bitmap：指向位图结构的指针。
        int index：要设置位的起始索引。
        int count：要设置位的数量。
        int bit：要设置的位的值（0或1）。
    */
    for(int i = 0; (i < count) && (index < bitmap->bit_count); i++, index++){
        if(bit){
            bitmap->bits[index/8] |= (1 << (index % 8));    //将目标位置设置为1
        }else{
            bitmap->bits[index/8] &= ~( 1 <<(index % 8));   //将目标位置设置为0
        }
    }
}
//判断是否设置成功
int bitmap_is_set (bitmap_t* bitmap, int index){
    return bitmap_get_bit(bitmap, index) ? 1 : 0;
}
//分配多个页
int bitmap_alloc_nbits(bitmap_t* bitmap, int bit, int count){
    int search_idx = 0;
    int ok_index = -1;
    while(search_idx < bitmap->bit_count){
        // 这里该用bitamp_get_bit吗？？？？？？？？？？？？？？？//这里就是一次判断一个页内存的大小了
        if(bitmap_get_bit(bitmap, search_idx) != bit){
            search_idx++;
            continue;
        }
        ok_index = search_idx;
        int i;
        for(i = 1; (i < count) && (i < bitmap->bit_count); i++){
            if(bitmap_get_bit(bitmap, search_idx++) != bit){
                ok_index = -1;
                break;
            }
        }
        if(i >= count){
            bitmap_set_bit(bitmap, ok_index, count, ~bit);
            return ok_index;
        }
    }
    return -1;

}
