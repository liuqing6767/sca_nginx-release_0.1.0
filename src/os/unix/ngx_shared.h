
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * 进程是计算机系统资源分配的最小单元。每个进程都有自己的资源，彼此隔离。
 * 内存是进程的私有资源，是虚拟内存，由操作系统映射到物理内存。
 * 由 malloc内存申请的内存，进程间的内存相互隔离。
 * 
 * 共享内存可以让多个进程将自己的某块虚拟内存映射到同一块物理内存，多个进程都能对同一块内存读/写.
 * linux 提供了 mmap 等函数。
 * 
 * 共享内存的写需要有锁的保护。
 * 
 * NGX 共享内存一般由master创建后记录共享内存的地址，worker继承父进程的共享内存地址后就可以进行共享内存的访问。
 * 
 * 
 * 这个版本共享内存使用的并不多的感觉
 */


#ifndef _NGX_SHARED_H_INCLUDED_
#define _NGX_SHARED_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


void *ngx_create_shared_memory(size_t size, ngx_log_t *log);


#endif /* _NGX_SHARED_H_INCLUDED_ */
