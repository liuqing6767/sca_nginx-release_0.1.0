
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * 核心数据结构：数组。其行为有：
 * - 创建：从内存池中分配 n * size 个元素
 * - push：拿到可以使用的 元素的地址。可能需要扩容
 * - 销毁：将内存释放
 * 
 */

#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


struct ngx_array_s {
    // elts 为 elements 数据块，指向实际存储的数据
    void        *elts;
    // nelts 为 number of elements
    ngx_uint_t   nelts;
    // size 为每个数据的大小
    size_t       size;
    // nalloc 为 已经分配的数据大小。就是数组可存储元素大小
    ngx_uint_t   nalloc;
    // pool 为当前数组的内存池。数组和elts都是从这个池中使用的内存
    ngx_pool_t  *pool;
};


ngx_array_t *ngx_create_array(ngx_pool_t *p, ngx_uint_t n, size_t size);
void ngx_destroy_array(ngx_array_t *a);
void *ngx_push_array(ngx_array_t *a);

// ngx_array_init 初始化array。主要是申请 n * size 的块
ngx_inline static ngx_int_t ngx_array_init(ngx_array_t *array, ngx_pool_t *pool,
                                           ngx_uint_t n, size_t size)
{
    if (!(array->elts = ngx_palloc(pool, n * size))) {
        return NGX_ERROR;
    }

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    return NGX_OK;
}


/*
 * ngx_init_array 初始化数组，是个宏。逻辑为：
 * 如果内存申请失败，返回 rc
 * 将申请到的内存初始化 a
 */
#define ngx_init_array(a, p, n, s, rc)                                       \
    ngx_test_null(a.elts, ngx_palloc(p, n * s), rc);                         \
    a.nelts = 0; a.size = s; a.nalloc = n; a.pool = p;

#define ngx_array_create  ngx_create_array
#define ngx_array_push    ngx_push_array


#endif /* _NGX_ARRAY_H_INCLUDED_ */
