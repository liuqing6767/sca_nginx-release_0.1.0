
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>

// nginx_create_pool 新建一个内存池
// size 就是这个池的总大小
ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log)
{
    ngx_pool_t  *p;

    if (!(p = ngx_alloc(size, log))) {
       return NULL;
    }

    p->last = (char *) p + sizeof(ngx_pool_t);
    p->end = (char *) p + size;
    p->next = NULL;
    p->large = NULL;
    p->log = log;

    return p;
}

// ngx_destroy_pool 释放整个池
void ngx_destroy_pool(ngx_pool_t *pool)
{
    ngx_pool_t        *p, *n;
    ngx_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                       "free: " PTR_FMT, l->alloc);

        if (l->alloc) {
            free(l->alloc);
        }
    }

#if (NGX_DEBUG)

    /*
     * we could allocate the pool->log from this pool
     * so we can not use this log while the free()ing the pool
     */

    for (p = pool, n = pool->next; /* void */; p = n, n = n->next) {
        ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                       "free: " PTR_FMT ", unused: " SIZE_T_FMT,
                       p, p->end - p->last);

        if (n == NULL) {
            break;
        }
    }

#endif

    for (p = pool, n = pool->next; /* void */; p = n, n = n->next) {
        free(p);

        if (n == NULL) {
            break;
        }
    }
}


// ngx_palloc 创建一个节点
// 内存在堆上
void *ngx_palloc(ngx_pool_t *pool, size_t size)
{
    char              *begin_position;
    ngx_pool_t        *cur_node, *next_node;

    // 如果是小块
    if (size <= (size_t) NGX_MAX_ALLOC_FROM_POOL
        && size <= (size_t) (pool->end - (char *) pool) - sizeof(ngx_pool_t))
    {
        // 看 pool->next，也就是链表中的当前元素
        for (cur_node = pool, next_node = pool->next; /* void */; cur_node = next_node, next_node = next_node->next) {
            // 定位到第一个对齐的位置
            begin_position = ngx_align(cur_node->last);

            // 如果当前节点的大小 ≥ 需要的内存大小， 查找成功，直接返回
            if ((size_t) (cur_node->end - begin_position) >= size) {
                cur_node->last = begin_position + size ;

                return begin_position;
            }

            if (next_node == NULL) {
                break;
            }
        }

        /* allocate a new pool block */

        // 没找到，只能分配出一个节点
        if (!(next_node = ngx_create_pool((size_t) (cur_node->end - (char *) cur_node), cur_node->log))) {
            return NULL;
        }

        cur_node->next = next_node;
        begin_position = next_node->last;
        next_node->last += size;

        return begin_position;
    }

    /* allocate a large block */

    ngx_pool_large_t  *large, *last;
    large = NULL;
    last = NULL;

    if (pool->large) {
        // 先尝试找到空的大块节点
        for (last = pool->large; /* void */ ; last = last->next) {
            if (last->alloc == NULL) {
                large = last;
                last = NULL;
                break;
            }

            if (last->next == NULL) {
                break;
            }
        }
    }

    // 如果没有找到，就分配一个
    if (large == NULL) {
        if (!(large = ngx_palloc(pool, sizeof(ngx_pool_large_t)))) {
            return NULL;
        }

        large->next = NULL;
    }

#if 0
    if (!(p = ngx_memalign(ngx_pagesize, size, pool->log))) {
        return NULL;
    }
#else
    // 申请一块大小正好的内存
    if (!(cur_node = ngx_alloc(size, pool->log))) {
        return NULL;
    }
#endif

    if (pool->large == NULL) {
        pool->large = large;

    } else if (last) {
        last->next = large;
    }

    large->alloc = cur_node;

    return cur_node;
}


// ngx_pfree 释放指定节点
ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p)
{
    ngx_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "free: " PTR_FMT, l->alloc);
            free(l->alloc);
            l->alloc = NULL;

            return NGX_OK;
        }
    }

    return NGX_DECLINED;
}


// ngx_pcalloc 同 ngx_palloc，只是会将内存初始化
void *ngx_pcalloc(ngx_pool_t *pool, size_t size)
{
    void *p;

    p = ngx_palloc(pool, size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}

#if 0

static void *ngx_get_cached_block(size_t size)
{
    void                     *p;
    ngx_cached_block_slot_t  *slot;

    if (ngx_cycle->cache == NULL) {
        return NULL;
    }

    slot = &ngx_cycle->cache[(size + ngx_pagesize - 1) / ngx_pagesize];

    slot->tries++;

    if (slot->number) {
        p = slot->block;
        slot->block = slot->block->next;
        slot->number--;
        return p;
    }

    return NULL;
}

#endif
