
/*
 * Copyright (C) Igor Sysoev
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

/*
 * ngx_cycle_s 是使用配置创建出来的 nginx运行时上下文。
 * 可以理解是个死循环的概念
 * 当前的周期由nginx worker继承
 * 每次配置加载时都会删除旧的cycle对象，创建新的cycle对象
 * 
 * 其包括多个 ngx_listening_t，多个 ngx_connection_t，多个读事件 + 写事件(和连接对应)
 */
struct ngx_cycle_s {
    // conf_ctx 保存所有模块配置项的结构体指针
    // 本身是一个数组，每个成员是模块的配置，每个模块的配置又是一个指针(void *)
    void           ****conf_ctx;
    // pool 是内存池
    ngx_pool_t        *pool;

    // log 是程序初始化还没解析参数时的临时log，会将日志输出到标准输出。后面会被 new_log 替换
    ngx_log_t         *log;
    // new_log 是正式的log对象。根据 error_log 指令来创建的
    ngx_log_t         *new_log;

    // listening 为监听器列表（监听池， ngx_listening_t 列表）
    ngx_array_t        listening;
    // pathes 保存所有需要的操作的目录。如果不存在会先创建，如果创建失败就启动失败
    ngx_array_t        pathes;
    // open_files 保存了所有的打开的文件
    ngx_list_t         open_files;

    ngx_uint_t         connection_n;
    // 整个连接池数组，大小为 connection_n。由 worker_connections 指令指定
    // 由event模块在初始化每个worker时创建的
    ngx_connection_t  *connections;
    // 与连接池对应的读事件池 数组，大小为 connection_n
    ngx_event_t       *read_events;
    // 与连接池对应的写事件池 的头，大小为 connection_n
    ngx_event_t       *write_events;

    // old_cycle 用于保存上一个对象。在启动初期需要建联一个临时的对象保存一些变量，就把这个传递进来
    ngx_cycle_t       *old_cycle;

    // 配置所在的路径
    ngx_str_t          conf_file;
    // 程序的根路径
    ngx_str_t          root;
};


typedef struct {
     ngx_flag_t  daemon;
     ngx_flag_t  master;

     ngx_int_t   worker_processes;

     ngx_uid_t   user;
     ngx_gid_t   group;

     ngx_str_t   pid;
     ngx_str_t   newpid;

#if (NGX_THREADS)
     ngx_int_t   worker_threads;
     size_t      thread_stack_size;
#endif

} ngx_core_conf_t;


typedef struct {
     ngx_pool_t  *pool;   /* pcre's malloc() pool */
} ngx_core_tls_t;


/*
 * ngx_init_cycle 创建新的cycle
 */
ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);
ngx_int_t ngx_create_pidfile(ngx_cycle_t *cycle, ngx_cycle_t *old_cycle);
void ngx_delete_pidfile(ngx_cycle_t *cycle);
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);


extern volatile ngx_cycle_t  *ngx_cycle;
extern ngx_array_t            ngx_old_cycles;
extern ngx_module_t           ngx_core_module;
extern ngx_uint_t             ngx_test_config;
#if (NGX_THREADS)
extern ngx_tls_key_t          ngx_core_tls_key;
#endif


#endif /* _NGX_CYCLE_H_INCLUDED_ */
