
/*
 * Copyright (C) Igor Sysoev
 */


#ifndef _NGX_PROCESS_CYCLE_H_INCLUDED_
#define _NGX_PROCESS_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

// master 会广播给 worker 进程的 消息类型

// 新建/发布一个通信管道
#define NGX_CMD_OPEN_CHANNEL   1
// 关闭一个通信管道
#define NGX_CMD_CLOSE_CHANNEL  2
// 平滑退出
#define NGX_CMD_QUIT           3
// 强制退出
#define NGX_CMD_TERMINATE      4
// 重新打开文件
#define NGX_CMD_REOPEN         5


typedef struct {
    int           argc;
    char *const  *argv;
} ngx_master_ctx_t;


/*
 * NGX 的进程类型
 *
 * Single：当配置 master_process off 时。是 Master + Worker
 * 多进程：
 *  - Master：读取NGX的配置，创建 循环，开始和控制子进程。不执行任何I/O，只对信号做出响应。使用 ngx_master_process_cycle
 *  - Worker：处理客户端请求。对信号和管道命令进行响应。可以有多个进程，用 worker_processes 指令配置。使用 ngx_worker_process_cycle
 */
#define NGX_PROCESS_SINGLE   0
#define NGX_PROCESS_MASTER   1
#define NGX_PROCESS_WORKER   2


void ngx_master_process_cycle(ngx_cycle_t *cycle, ngx_master_ctx_t *ctx);
void ngx_single_process_cycle(ngx_cycle_t *cycle, ngx_master_ctx_t *ctx);


extern ngx_uint_t      ngx_process;
extern ngx_pid_t       ngx_pid;
extern ngx_pid_t       ngx_new_binary;
extern ngx_uint_t      ngx_inherited;
extern ngx_uint_t      ngx_daemonized;
extern ngx_uint_t      ngx_threaded;
extern ngx_uint_t      ngx_exiting;

extern sig_atomic_t    ngx_reap;
extern sig_atomic_t    ngx_timer;
extern sig_atomic_t    ngx_sigio;
extern sig_atomic_t    ngx_quit;
extern sig_atomic_t    ngx_terminate;
extern sig_atomic_t    ngx_noaccept;
extern sig_atomic_t    ngx_reconfigure;
extern sig_atomic_t    ngx_reopen;
extern sig_atomic_t    ngx_change_binary;


#endif /* _NGX_PROCESS_CYCLE_H_INCLUDED_ */
