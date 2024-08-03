
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>

/*
 * ngx_create_array 从内存池p中创建数组。先申请 数组的内存，再申请数组的 elts 的内存地址
 */
ngx_array_t *ngx_create_array(ngx_pool_t *p, ngx_uint_t n, size_t size)
{
    ngx_array_t *a;

    ngx_test_null(a, ngx_palloc(p, sizeof(ngx_array_t)), NULL);

    ngx_test_null(a->elts, ngx_palloc(p, n * size), NULL);

    a->pool = p;
    a->nelts = 0;
    a->nalloc = n;
    a->size = size;

    return a;
}


/*
 * ngx_destroy_array 销毁数组。将内存还给 pool，而不还给操作系统
 */
void ngx_destroy_array(ngx_array_t *a)
{
    ngx_pool_t  *p;

    p = a->pool;

    if ((char *) a->elts + a->size * a->nalloc == p->last) {
        p->last -= a->size * a->nalloc;
    }

    if ((char *) a + sizeof(ngx_array_t) == p->last) {
        p->last = (char *) a;
    }

    // TODO 如果不在pool的最末尾，怎么还给pool呢？
}

/*
 * ngx_push_array 拿到可以使用的下一个元素的地址
 */
void *ngx_push_array(ngx_array_t *a)
{
    void        *elt, *new;
    ngx_pool_t  *p;

    /* array is full */
    if (a->nelts == a->nalloc) {
        p = a->pool;

        /* array allocation is the last in the pool */
        if (
            (char *) a->elts + a->size * a->nelts == p->last && 
            (unsigned) (p->end - p->last) >= a->size // pool 上面还有一倍的空间，直接拿来用(避免元素拷贝)
        ) {
            p->last += a->size;
            a->nalloc++;

        /* allocate new array */
        } else {
            ngx_test_null(new, ngx_palloc(p, 2 * a->nalloc * a->size), NULL);

            ngx_memcpy(new, a->elts, a->nalloc * a->size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}
