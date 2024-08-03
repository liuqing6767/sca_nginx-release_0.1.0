
/*
 * Copyright (C) Igor Sysoev
 */


#ifndef _NGX_PROCESS_H_INCLUDED_
#define _NGX_PROCESS_H_INCLUDED_


typedef pid_t       ngx_pid_t;

typedef void (*ngx_spawn_proc_pt) (ngx_cycle_t *cycle, void *data);

/* 进程 */
typedef struct {
    // 进程ID
    ngx_pid_t           pid;
    // 进程状态
    int                 status;

    // channel是派生进程时通过socketpair得到的，子进程创建时继承得到。用于master和worker之间的通信
    // master 写入channel[0], worker 读取 channel[1]
    ngx_socket_t        channel[2];

    // 子进程循环方法。比如 worker进程的 ngx_worker_process_cycle
    ngx_spawn_proc_pt   proc;
    // 派生子进程后，执行 proc(cycle, data)
    void               *data;
    // 进程名
    char               *name;

    // 1表示子进程受master进程管理，子进程挂了可以复活
    unsigned            respawn:1;
    // 1表示刚刚派生的子进程。在热加载配置文件时会用到
    unsigned            just_respawn:1;
    // 1表示游离的子进程。在升级binary时会派生一个新的 游离的 Master进程
    unsigned            detached:1;
    // 1表示正在主动退出。区别与异常退出
    unsigned            exiting:1;
    // 1表示已经退出
    unsigned            exited:1;
} ngx_process_t;


typedef struct {
    char         *path;
    char         *name;
    char *const  *argv;
    char *const  *envp;
} ngx_exec_ctx_t;


#define NGX_MAX_PROCESSES         1024

#define NGX_PROCESS_NORESPAWN     -1
#define NGX_PROCESS_RESPAWN       -2
#define NGX_PROCESS_JUST_RESPAWN  -3
#define NGX_PROCESS_DETACHED      -4


#define ngx_getpid   getpid
#define ngx_log_pid  ngx_pid

ngx_pid_t ngx_spawn_process(ngx_cycle_t *cycle,
                            ngx_spawn_proc_pt proc, void *data,
                            char *name, ngx_int_t respawn);
ngx_pid_t ngx_execute(ngx_cycle_t *cycle, ngx_exec_ctx_t *ctx);
void ngx_process_get_status(void);

#define ngx_sched_yield()  sched_yield()


extern ngx_pid_t      ngx_pid;
extern ngx_socket_t   ngx_channel;
extern ngx_int_t      ngx_process_slot;
extern ngx_int_t      ngx_last_process;
extern ngx_process_t  ngx_processes[NGX_MAX_PROCESSES];


#endif /* _NGX_PROCESS_H_INCLUDED_ */
