
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * 基数是一种二叉查找树。
 * 每个节点存储的都是32位的整数类型数据。
 * 插入节点时，现将key转换为32位的二进制数据，从左往右 遇0插入左，遇1插入右
 * 
 * 没发现哪里用来啊？
 */

#ifndef _NGX_RADIX_TREE_H_INCLUDED_
#define _NGX_RADIX_TREE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_radix_node_s  ngx_radix_node_t;

struct ngx_radix_node_s {
    ngx_radix_node_t  *right;
    ngx_radix_node_t  *left;
    ngx_radix_node_t  *parent;
    uintptr_t          value;
};


typedef struct {
    // 根节点
    ngx_radix_node_t  *root;

    // 所有的内存都是从pool里面拿出来的
    ngx_pool_t        *pool;

    // 已经分配但是没有使用的节点
    ngx_radix_node_t  *free;
    // 已经分配但是没有使用的内存的首地址
    char              *start;
    // 已经分配但是没有使用的内存大小
    size_t             size;
} ngx_radix_tree_t;


ngx_radix_tree_t *ngx_radix_tree_create(ngx_pool_t *pool);
ngx_int_t ngx_radix32tree_insert(ngx_radix_tree_t *tree,
                                 uint32_t key, uint32_t mask, uintptr_t value);
ngx_int_t ngx_radix32tree_delete(ngx_radix_tree_t *tree,
                                 uint32_t key, uint32_t mask);
uintptr_t ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key);


#endif /* _NGX_RADIX_TREE_H_INCLUDED_ */
