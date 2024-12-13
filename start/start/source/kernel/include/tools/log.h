#ifndef LOG_H
#define LOG_H

//初始化接口，硬件相关的初始化工作
void log_init(void);

//printf接口函数    ...可变参数，数量没有限�?
void log_printf(const char * fmt, ...);

#endif