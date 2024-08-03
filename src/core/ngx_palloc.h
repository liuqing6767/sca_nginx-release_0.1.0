
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * 核心数据结构：内存池
 * NGX 提供了两类的内存池：
 * - ngx_pool_large_s：为内存块的链表。这个按需实时申请内存
 * - ngx_pool_s：也是内存块的链表。但是每个内存块一次申请，可以被多次给出去
 */

#ifndef _NGX_PALLOC_H_INCLUDED_
#define _NGX_PALLOC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * NGX_MAX_ALLOC_FROM_POOL should be (ngx_page_size - 1), i.e. 4095 on x86.
 * On FreeBSD 5.x it allows to use the zero copy sending.
 * On Windows NT it decreases a number of locked pages in a kernel.
 */
#define NGX_MAX_ALLOC_FROM_POOL  (ngx_pagesize - 1)

#define NGX_DEFAULT_POOL_SIZE   (16 * 1024)

#define ngx_test_null(p, alloc, rc)  if ((p = alloc) == NULL) { return rc; }


typedef struct ngx_pool_large_s  ngx_pool_large_t;

// ngx_pool_large_s 是大块内存 链表 / 节点
struct ngx_pool_large_s {
    // next 指向下一个节点
    ngx_pool_large_t  *next;

    // alloc 为数据的内存地址
    void              *alloc;
};


/* ngx_pool_s 是一个 内存池。
 * 对于小块，一次性申请大量内存后分批给出去；
 * 对于大块, 需要时申请
 *
           +----------------+                            +----------------+
           |   small block  |                            |   small block  |
           +---^------------+                            +---------^------+
+------+       |            ^                 +------+             |      ^
| last +-------+            |                 | last +-------------+      |               
|      |                    |                 |      |                    |
| end  +--------------------+                 | end  +--------------------+
|      |                                      |      |
| next +------------------------------------->| next +----------------------------------------->  more
+------+                                      +------+
|      |      +------+                        |      |      +------+
|large +------> next +---> more               |large +------> next +---> more
+------+      |      |     +-------------+    +------+      |      |     +-------------+
              |alloc +---->|             |                  |alloc +---->|             |
              +------+     |  big block  |                  +------+     |  big block  |
                           |             |                               |             |
                           +-------------+                               +-------------+
*/
struct ngx_pool_s {
    // last - end 之间的内存只申请一次，多次给使用方没被使用部分
    // 小数据 起始的地址。每次分配时，如果 end-last >= 期望的大小，则直接使用
    char              *last;
    // 小数据 结束的地址
    char              *end;
    // next 为存储普通大小的数据的链表 的下一个节点 。这个一定存在
    ngx_pool_t        *next;

    // large 为存储大数据的链表 的下一个节点。这个可能不存在
    ngx_pool_large_t  *large;
    // 大块还是小块 由 NGX_MAX_ALLOC_FROM_POOL 来定，不同的操作系统不一样。当申请的内存大于这个值时，会按大块内存进行分配

    // lag 为日志对象
    ngx_log_t         *log;
};


/*
 * ngx_alloc 申请内存
 */
void *ngx_alloc(size_t size, ngx_log_t *log);

/*
 * ngx_calloc 申请内存并且初始化
 */
void *ngx_calloc(size_t size, ngx_log_t *log);

/*
 * ngx_create_pool 创建一个池（内存块的链表）
 */
ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log);
void ngx_destroy_pool(ngx_pool_t *pool);

/*
 * ngx_palloc 创建一个 size大小的内存块，返回开头的地址
 */
void *ngx_palloc(ngx_pool_t *pool, size_t size);
/*
 * ngx_pcalloc 创建一个 size大小的内存块并初始化，返回开头的地址
 */
void *ngx_pcalloc(ngx_pool_t *pool, size_t size);

/*
 * ngx_pfree 是否指定的内存节点
 */
ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p);


#endif /* _NGX_PALLOC_H_INCLUDED_ */
