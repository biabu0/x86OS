#ifndef LIST_H
#define LIST_H

#include "comm/types.h"

typedef struct _list_node_t {
    struct _list_node_t * pre;
    struct _list_node_t * next;
} list_node_t;
static inline void list_node_init(list_node_t * node) {
    node->pre = node->next = (list_node_t *)0;
}
static inline list_node_t * list_node_pre (list_node_t * node){
    return node->pre;
}
static inline list_node_t * list_node_next (list_node_t * node){
    return node->next;
}
typedef struct _list_t {
    list_node_t * first;
    list_node_t * last;
    int count;      //记录节点个数
} list_t;
void list_init(list_t * list);
static inline int list_is_empty(list_t * list) {
    return list->count == 0;
}
static inline int list_count(list_t * list) {
    return list->count;
}
static inline list_node_t * list_first(list_t * list) {
    return list->first;
}
static inline list_node_t * list_last(list_t * list) {
    return list->last;
}

void list_insert_first(list_t * list, list_node_t * node);
void list_insert_last(list_t * list, list_node_t * node);

//返回值类型为list_node_t，原因是为了后续删除节点返回为空
list_node_t * list_remove_first(list_t * list);
list_node_t * list_remove(list_t * list, list_node_t * node);

// parent_type为待求地址的结构体类型，node_name为待求地址的结构体中定义的已知地址的变量名称，node为取出的已知变量
//这个宏利用了编译器的能力来计算结构体成员的偏移量
#define offset_in_parent(parent_type, node_name) \
    (uint32_t)(&(((parent_type *)0)->node_name))
#define parent_addr(node, parent_type, node_name) \
    ((uint32_t)node - offset_in_parent(parent_type, node_name))
#define list_node_parent(node, parent_type, node_name) \
    ((parent_type *)(node ? parent_addr(node, parent_type, node_name):0))

#endif