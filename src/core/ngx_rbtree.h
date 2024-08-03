
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * 红黑树是每个节点都带有颜色属性的二叉查找树。
 * 它是自平衡的二叉树，在插入和删除时能够保持二叉树的平衡性，从而获得较高的查找性能。
 * 有如下属性：
 * - 节点是黑色或红色
 * - 根节点是黑色，叶子节点是黑色
 * - 红色节点必须有两个黑色节点
 * - 任意节点到每个叶子节点的所有路径都包含相同数目的黑色节点
 * 
 * NGX 中用来维护定时器
 */

#ifndef _NGX_RBTREE_H_INCLUDED_
#define _NGX_RBTREE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_rbtree_s  ngx_rbtree_t;

struct ngx_rbtree_s {
   ngx_int_t       key;
   ngx_rbtree_t   *left;
   ngx_rbtree_t   *right;
   ngx_rbtree_t   *parent;
   char            color;
};


void ngx_rbtree_insert(ngx_rbtree_t **root, ngx_rbtree_t *sentinel,
                       ngx_rbtree_t *node);
void ngx_rbtree_delete(ngx_rbtree_t **root, ngx_rbtree_t *sentinel,
                       ngx_rbtree_t *node);


ngx_inline static ngx_rbtree_t *ngx_rbtree_min(ngx_rbtree_t *node,
                                               ngx_rbtree_t *sentinel)
{
   while (node->left != sentinel) {
       node = node->left;
   }

   return node;
}


#endif /* _NGX_RBTREE_H_INCLUDED_ */
