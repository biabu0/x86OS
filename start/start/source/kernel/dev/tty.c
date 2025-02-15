#include "dev/tty.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "dev/kbd.h"
#include "dev/console.h"

//中断和进程都有可能对fifo进行读写（键盘中断写数据），锁对中断的情况是不能保护的，故使用中断处理
#include "cpu/irq.h"


//存在多个tty设备
static tty_t tty_devs[TTY_NR];

static int curr_tty = 0;     //当前正在使用的tty，默认0

void tty_fifo_init(tty_fifo_t * fifo, char * buf, int size){
    fifo->buf = buf;
    fifo->size = size;
    fifo->read = 0;
    fifo->write = 0;
    fifo->count = 0;
}

int tty_fifo_put(tty_fifo_t * fifo, char c){
    irq_state_t state = irq_enter_protection();
    //缓存区域已经满了
    if(fifo->count >= fifo->size){
        irq_leave_protection(state);
        return -1;
    }
    //向buf中写入字符
    fifo->buf[fifo->write++] = c;
    if(fifo->write >= fifo->size){
        fifo->write = 0;
    }
    fifo->count++;
    irq_leave_protection(state);
    return 0;
}

int tty_fifo_get(tty_fifo_t * fifo, char * c){
    irq_state_t state = irq_enter_protection();
    if(fifo->count <= 0){
        irq_leave_protection(state);
        return -1;
    }
    *c = fifo->buf[fifo->read++];
    if(fifo->read >= fifo->size){
        fifo->read = 0;
    }
    fifo->count--;
    irq_leave_protection(state);
    return 0;
}

static tty_t * get_tty(device_t * dev){
    int tty = dev->minor;
    if((tty < 0) || (tty >= TTY_NR) || (!dev->open_count)){
        log_printf("tty is not opened. tty = %d", tty);
        return (tty_t*)0;
    }
    return tty_devs + tty;
}

//操作系统只需要调用这个open即可
int tty_open(device_t *dev){
    int idx = dev->minor;
    if((idx < 0) || idx >= TTY_NR){
        log_printf("open tty failed. incorrect tty num = %d", idx);
        return -1;
    }
    tty_t * tty = tty_devs + idx;
    tty_fifo_init(&tty->ofifo, tty->obuf, TTY_OBUF_SIZE);
    //初始化信号量，用于当缓冲区满的时候阻塞进程
    sem_init(&tty->osem, TTY_OBUF_SIZE);
    tty_fifo_init(&tty->ififo, tty->ibuf, TTY_IBUF_SIZE);
    tty->oflags = TTY_OCRLF;                        //默认开启\n是回车加换行
    //向第idx块显存区域中写数据显示在显示屏上
    tty->console_idx = idx;
    sem_init(&tty->isem, 0);//最开始输入缓冲区内容为0，阻塞进程，当中断输入数据的时候，唤醒进程
    tty->iflags = TTY_INCLR | TTY_IECHO;            //默认开启回显和回车加换行
    kbd_init();//键盘初始化，使用一个静态变量限定只初始化一次

    //初始化这一块显示区域
    console_init(idx);

    return 0; 
}
int tty_write(device_t * dev, int addr, char * buf, int size){
    if(size < 0){
        return -1;
    }
    tty_t * tty = get_tty(dev);
    if(!tty){
        return -1;
    }
    int len = 0;
    while(size){
        char c = *buf++;
        //如果是'\n'，应该是回车加换行，在linux中\n直接就是回到下一行的首位，故先写入一个回车字符，之后在将\n写入也就实现了\r\n，并没有直接在console中实现该功能
        //通过一个可以选择的开关来确定\n的具体功能
        if((c == '\n') && (tty->oflags & TTY_OCRLF)){
            sem_wait(&tty->osem);
            int err = tty_fifo_put(&tty->ofifo, '\r');
            if(err < 0){
                break;
            }
        }
        //初始化了一个信号量，并将其初始值设置为输出缓冲区的大小（TTY_OBUF_SIZE）。
        //这表示缓冲区一开始是空的，可以有 TTY_OBUF_SIZE 个数据项被写入。
        sem_wait(&tty->osem);
        int err = tty_fifo_put(&tty->ofifo, c);
        if(err < 0){
            log_printf("tty_write is failed.");
            break;
        }
        len ++;
        size --;
        /*
        if(显示器正在忙){
            continue;
        }else{
            启动硬件发送显示
        }
        */ 
       console_write(tty);

    }
    return len;
}
int tty_read(device_t * dev, int addr, char * buf, int size){
    if(size < 0){
        return -1;
    }
    tty_t * tty = get_tty(dev);
    char * pbuf = buf;
    int len = 0;
    while(len < size){
        sem_wait(&tty->isem);
        char ch;
        tty_fifo_get(&tty->ififo, &ch);
        switch(ch){
            //0x7F是退格键，退格之后也应该在tty设备的缓冲区中删除
            case 0x7F:
                if(len == 0){
                    continue;
                }
                len--;
                pbuf--;
                break;
            case '\n':
                if((len < size-1) && (tty->iflags & TTY_INCLR)){
                    *pbuf++ = '\r';
                    len++;
                }
                *pbuf++ = '\n';
                len ++;  
                break;
            default:
                *pbuf++ = ch;
                len++;
                break;
        }
        if(tty->iflags & TTY_IECHO){
            tty_write(dev, 0, &ch, 1);      //回显
        }
        if((ch == '\n') || (ch == '\r')){       //遇到换行或者回车，结束输入
            break;
        }
    }
    return len;
}

//设备特定的操作
int tty_control(device_t * dev, int cmd, int arg0, int arg1){
    return 0;
}
void tty_close(device_t * dev){

}

void tty_in(char ch){
    tty_t * tty = tty_devs + curr_tty;       //当前的tty设备
    if(sem_count(&tty->isem) >= TTY_IBUF_SIZE){
        return ;
    }

    tty_fifo_put(&tty->ififo, ch);
    sem_notify(&tty->isem);
}

void tty_select(int tty){
    if(tty != curr_tty){
        console_select(tty);           //硬件显示的数据切换到相应的显存位置
        curr_tty = tty;
    }
}

dev_desc_t dev_tty_desc = {
    .name = "tty",
    .major = DEV_TTY,
    .open = tty_open,
    .read = tty_read,
    .write = tty_write,
    .control = tty_control,
    .close = tty_close
};