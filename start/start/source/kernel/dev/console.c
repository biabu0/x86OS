#include "dev/console.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "dev/tty.h"
#include "cpu/irq.h"

#define CONSOLE_NR 8
static console_t console_buf[CONSOLE_NR];
static int curr_console_id = 0;

//获取当前光标位置
static int read_cursor_pos(void){
    int pos;
    irq_state_t state = irq_enter_protection();
    outb(0x3D4, 0xF);
    pos = inb(0x3d5);
    outb(0x3D4, 0xE);
    pos |= inb(0x3d5) << 8;
    irq_leave_protection(state);
    return pos;
}


//更新光标位置
static int update_cursor_pos(console_t * console){
    //获取位置
    uint16_t pos = (console- console_buf) * console->display_rows * console->display_cols;
    pos += console->cursor_row * console->display_cols + console->cursor_col;
    //向端口写入光标的位置
    irq_state_t state = irq_enter_protection();
    outb(0x3D4, 0xF);
    outb(0x3d5,(uint8_t)(pos&0xFF));
    outb(0x3D4, 0xE);
    outb(0x3d5, (uint8_t)((pos >> 8) & 0xFF));
    irq_leave_protection(state);
    return pos;
}

static void erase_rows(console_t * console, int start, int end){
    disp_char_t * disp_start = console->disp_base + start * console->display_cols;\
    disp_char_t * disp_end = console->disp_base + (end + 1) * console->display_cols;
    while(disp_start < disp_end){
        disp_start->c = ' ';
        disp_start->foreground = console->foreground;
        disp_start->background = console->background;
        disp_start++;
    }
}

static void scroll_up(console_t * console, int lines){
    disp_char_t * dest = console->disp_base;
    disp_char_t * src = console->disp_base + lines * console->display_cols;
    uint32_t size = (console->display_rows - lines) * console->display_cols * sizeof(disp_char_t);
    kernel_memcpy(dest, src, size);
    erase_rows(console, console->display_rows-lines, console->display_rows-1);
    console->cursor_row -= lines;
}


static void move_to_col0(console_t * console){
    console->cursor_col = 0;
}

static void move_next_line(console_t * console){
    console->cursor_row++;;
    if(console->cursor_row >= console->display_rows){
        scroll_up(console, 1);
    }
}

static void move_forward(console_t * console, int n){
    for(int i = 0; i < n; i++){
        if(++console->cursor_col >= console->display_cols){
            console->cursor_col = 0;
            console->cursor_row++;
            //行如果超过最大则需要上滚
            if(console->cursor_row >= console->display_rows){
                scroll_up(console, 1);
            }
        }
    }
}
static void show_char(console_t * console, char ch){
    int offset = console->cursor_col + console->cursor_row * console->display_cols;
    disp_char_t * p = console->disp_base + offset;
    //显示字符，在p位置写入字符和属性
    p->c = ch;
    p->foreground = console->foreground;
    p->background = console->background;
    //光标后移
    move_forward(console, 1);

}

static int move_backward(console_t * console, int n){
    int status = -1;

    for(int i = 0; i < n; i++){
        if(console->cursor_col > 0){
            console->cursor_col--;
            status = 0;
        }else if(console->cursor_row > 0){
            console->cursor_row--;
            console->cursor_col = console->display_cols - 1;
            status = 0;
        }
    }
    return status;
}

static void erase_backward(console_t * console){
    if(move_backward(console, 1) == 0){
        show_char(console, ' ');
        move_backward(console, 1);
    }
}
static void clear_display(console_t * console){
    disp_char_t * start = console->disp_base; 
    int size = console->display_rows * console->display_cols;
    for(int i = 0; i < size; i++){
        start->c = ' ';
        start->foreground = console->foreground;
        start->background = console->background;
        start++;
    }
}
int console_init(int idx){
    
        console_t * console = console_buf + idx;
        //不再清空，可能有boot和loader打印的信息
        // console->cursor_row = 0;
        // console->cursor_col = 0;
        console->display_rows = CONSOLE_ROW_MAX;
        console->display_cols = CONSOLE_COL_MAX;
        console->disp_base = (disp_char_t *)CONSOLE_DISP_ADDR + idx * (CONSOLE_COL_MAX * CONSOLE_ROW_MAX);
        
        console->foreground = CONSOLE_WHITE;
        console->background = CONSOLE_BLACK;
        if(idx == 0){
            int cursor_pos = read_cursor_pos();
            console->cursor_row = cursor_pos / console->display_cols;
            console->cursor_col = cursor_pos % console->display_cols;
        }else{
            console->cursor_col = 0;
            console->cursor_row = 0;
            clear_display(console);
            //update_cursor_pos(console);
        }
        console->old_cursor_col = console->cursor_col;
        console->old_cursor_row = console->cursor_row;
        console->write_state = CONSOLE_WRITE_NORMAL;
        //clear_display(console);
    
    return 0;
}

static void write_norm(console_t * c, char ch){

    switch(ch){
        case ASCII_ESC:
            c->write_state = CONSOLE_WRITE_ESC;
            break;
        case '\r':
            move_to_col0(c);
            break;
        case '\n':
            move_next_line(c);//移动到下一行
            break;
        case '\b':
            move_backward(c, 1);
            break;
        case 0x7F:
            erase_backward(c);
            break;
        case '\t':
            move_to_col0(c);
            break;
        default:
            if((ch >= ' ')&&(ch <= '~')){
                //显示字符
                show_char(c, ch);
            }
            break;
    }
}
void save_cursor(console_t * console){
    console->old_cursor_col = console->cursor_col;
    console->old_cursor_row = console->cursor_row;
}

void restore_cursor(console_t * console){
    console->cursor_col = console->old_cursor_col;
    console->cursor_row = console->old_cursor_row;
}
static void clear_esc_param (console_t * console) {
	kernel_memset(console->esc_param, 0, sizeof(console->esc_param));
	console->curr_param_index = 0;
}
static void set_font_style (console_t * console) {
	static const color_t color_table[] = {
			CONSOLE_BLACK, CONSOLE_RED, CONSOLE_GREEN, CONSOLE_YELLOW, // 0-3
			CONSOLE_BLUE, CONSOLE_MAGENTA, CONSOLE_CYAN, CONSOLE_WHITE, // 4-7
	};

	for (int i = 0; i < console->curr_param_index; i++) {
		int param = console->esc_param[i];
		if ((param >= 30) && (param <= 37)) {  // 前景色：30-37
			console->foreground = color_table[param - 30];
		} else if ((param >= 40) && (param <= 47)) {
			console->background = color_table[param - 40];
		} else if (param == 39) { // 39=默认前景色
			console->foreground = CONSOLE_WHITE;
		} else if (param == 49) { // 49=默认背景色
			console->background = CONSOLE_BLACK;
		}
	}
}
/**
 * @brief 光标左移，但不起始左边界，也不往上移
 */
static void move_left (console_t * console, int n) {
    // 至少移致动1个
    if (n == 0) {
        n = 1;
    }

    int col = console->cursor_col - n;
    console->cursor_col = (col >= 0) ? col : 0;
}

/**
 * @brief 光标右移，但不起始右边界，也不往下移
 */
static void move_right (console_t * console, int n) {
    // 至少移致动1个
    if (n == 0) {
        n = 1;
    }

    int col = console->cursor_col + n;
    if (col >= console->display_cols) {
        console->cursor_col = console->display_cols - 1;
    } else {
        console->cursor_col = col;
    }
}

/**
 * 移动光标
 */
static void move_cursor(console_t * console) {
	if (console->curr_param_index >= 1) {
		console->cursor_row = console->esc_param[0];
	}

	if (console->curr_param_index >= 2) {
		console->cursor_col = console->esc_param[1];
	}
}

/**
 * 擦除字符操作
 */
static void erase_in_display(console_t * console) {
	if (console->curr_param_index <= 0) {
		return;
	}

	int param = console->esc_param[0];
	if (param == 2) {
		// 擦除整个屏幕
		erase_rows(console, 0, console->display_rows - 1);
        console->cursor_col = console->cursor_row = 0;
	}
}
/**
 * @brief 处理ESC [Pn;Pn 开头的字符串
 */
static void write_esc_square (console_t * console, char c) {
    // 接收参数
    if ((c >= '0') && (c <= '9')) {
        // 解析当前参数
        int * param = &console->esc_param[console->curr_param_index];
        *param = *param * 10 + c - '0';
    } else if ((c == ';') && console->curr_param_index < ESC_PARAM_MAX) {
        // 参数结束，继续处理下一个参数
        console->curr_param_index++;
    } else {
        // 结束上一字符的处理
        console->curr_param_index++;

        // 已经接收到所有的字符，继续处理
        switch (c) {
        case 'm': // 设置字符属性
            set_font_style(console);
            break;
        case 'D':	// 光标左移n个位置 ESC [Pn D
            move_left(console, console->esc_param[0]);
            break;
        case 'C':
            move_right(console, console->esc_param[0]);
            break;
        case 'H':
        case 'f':
            move_cursor(console);
            break;
        case 'J':
            erase_in_display(console);
            break;
        }
        console->write_state = CONSOLE_WRITE_NORMAL;
    }
}

static void write_esc(console_t * console, char ch){
    switch(ch){
        case '7':
            save_cursor(console);
            console->write_state = CONSOLE_WRITE_NORMAL;
            break;
        case '8':
            restore_cursor(console);
            console->write_state = CONSOLE_WRITE_NORMAL;
            break;
        case '[':
            clear_esc_param(console);
            console->write_state = CONSOLE_WRITE_SQUARE;
            break;
        default:
            console->write_state = CONSOLE_WRITE_NORMAL;
            break;
    }
}


//向屏幕console写data
int console_write(tty_t * tty){
    //第console块屏幕的显存位置
    int console = tty->console_idx;
    console_t * c = console_buf + console;
    int len = 0;
    do{
        char ch;
        int err = tty_fifo_get(&tty->ofifo, &ch);
        if(err < 0){
            break;
        }
        //通知
        sem_notify(&tty->osem);
        switch(c->write_state){
            case CONSOLE_WRITE_NORMAL:
                write_norm(c, ch);
                break;
            case CONSOLE_WRITE_ESC:
                write_esc(c, ch);
                break;
            case CONSOLE_WRITE_SQUARE:
                write_esc_square(c, ch);
                break;
            default:
                break;
        }
        len++;
    }while(1);
    if(tty->console_idx == curr_console_id){
        update_cursor_pos(c);
    }
    
    return len;
}
void console_close(int console){
    //
}

void console_select(int idx){
    console_t * console = console_buf + idx;
    if(console->disp_base == 0){    //当前设备没有打开的时候
        console_init(idx);
    }
    uint16_t pos = idx * console->display_rows * console->display_cols;

    outb(0x3D4, 0xC);                       //写高地址
    outb(0x3D5, (uint8_t)(pos >> 8) & 0xFF);
    outb(0x3D4, 0xD);                       //写低地址
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    curr_console_id = idx;
    update_cursor_pos(console);

    // char num = idx + '0';
    // show_char(console, num);
}