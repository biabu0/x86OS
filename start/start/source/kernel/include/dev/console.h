#ifndef CONSOLE_H
#define CONSOLE_H

#include "comm/types.h"
#include "dev/tty.h"
#include "ipc/mutex.h"


#define CONSOLE_DISP_ADDR   0xb8000
#define CONSOLE_DISP_END    (0xb8000 + 32 * 1024)
#define CONSOLE_ROW_MAX     25
#define CONSOLE_COL_MAX     80


#define ESC_PARAM_MAX         10
#define ASCII_ESC           0x1b

typedef enum {
    CONSOLE_BLACK = 0, CONSOLE_BLUE, CONSOLE_GREEN, CONSOLE_CYAN,
    CONSOLE_RED, CONSOLE_MAGENTA, CONSOLE_BROWN, CONSOLE_LIGHT_GRAY,
    CONSOLE_DARK_GRAY, CONSOLE_LIGHT_BLUE, CONSOLE_LIGHT_GREEN,
    CONSOLE_LIGHT_CYAN, CONSOLE_LIGHT_RED,COLOR_LIGHT_MAGENTA, CONSOLE_YELLOW,
    CONSOLE_WHITE
}color_t;


//描述屏幕字符结构，两个字节，一个显示字符，一个显示设置
typedef union _disp_char_t{
    struct {
        char c;
        char foreground : 4;
        char background :3;
    };
    uint16_t v;
}disp_char_t;


// 对ESC开头的命令进行进行处理
// ESC [p0;p1m
typedef struct _console_t {
    enum{
        CONSOLE_WRITE_NORMAL,
        CONSOLE_WRITE_ESC,
        CONSOLE_WRITE_SQUARE,
    }write_state;
    disp_char_t * disp_base;
    //光标所在行和列
    int cursor_row, cursor_col;
    int display_rows, display_cols;
    //枚举类型，用来描述屏幕字符的显示颜色
    color_t foreground, background;
    int old_cursor_col, old_cursor_row;
    int esc_param[ESC_PARAM_MAX];
    int curr_param_index;
    mutex_t mutex;
}console_t;

int console_init(int idx);
//向屏幕console写data
int console_write(tty_t * tty);
void console_close(int console);

void console_select(int idx);
#endif