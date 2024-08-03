
/*
 * Copyright (C) Igor Sysoev
 */


#ifndef _NGX_LIST_H_INCLUDED_
#define _NGX_LIST_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


// ngx_list_part_t 为链表元素
typedef struct ngx_list_part_s  ngx_list_part_t;

// ngx_list_part_s 为链表元素
struct ngx_list_part_s {
    // elts 指向数据的实际存储区域
    void             *elts;
    // nelts 为 number of 已经分配的 elements
    ngx_uint_t        nelts;
    // next 为下一个节点
    ngx_list_part_t  *next;
};

// ngx_list_t 为链表头部
typedef struct {
    // last 为链表的最后一个节点
    ngx_list_part_t  *last;
    // part 链表的第一个节点
    ngx_list_part_t   part;
    // size 为每个节点的数据大小
    size_t            size;
    // nalloc 为每个节点包含的数据个数
    ngx_uint_t        nalloc;
    // pool 为链表头部所在的内存池
    ngx_pool_t       *pool;
} ngx_list_t;


ngx_inline static ngx_int_t ngx_list_init(ngx_list_t *list, ngx_pool_t *pool,
                                          ngx_uint_t n, size_t size)
{
    if (!(list->part.elts = ngx_palloc(pool, n * size))) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}


/*
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */


void *ngx_list_push(ngx_list_t *list);


#endif /* _NGX_LIST_H_INCLUDED_ */
