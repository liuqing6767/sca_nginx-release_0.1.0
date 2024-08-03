
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * 基本数据结构 1.1：时间
 * 时间相关的逻辑为比较常规的算法，不进一步解释
 */

#ifndef _NGX_TIMES_H_INCLUDED_
#define _NGX_TIMES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


void ngx_time_init();
void ngx_time_update(time_t s);
size_t ngx_http_time(u_char *buf, time_t t);
size_t ngx_http_cookie_time(u_char *buf, time_t t);
void ngx_gmtime(time_t t, ngx_tm_t *tp);

#if (NGX_THREADS)
ngx_int_t ngx_time_mutex_init(ngx_log_t *log);
#endif

#if (NGX_THREADS && (TIME_T_SIZE > SIG_ATOMIC_T_SIZE))

#define ngx_time()        *ngx_cached_time
extern volatile time_t    *ngx_cached_time;

#else

#define ngx_time()         ngx_cached_time
extern volatile time_t     ngx_cached_time;

#endif


/*
 * 基础语法：extern 关键字
 * 用来说明 此变量/函数时别处定义的，要在此处引用。编译器会在链接阶段将其与其他文件的定义进行关联
 * 只是声明，说明变量的性质，而不是定义，不分配内存
 * 
 * 如果要在定义之前使用，就需要加关键字 extern 进行声明
 * 
 * C语言中的变量和函数有数据类型和存储类型两个属性。存储类型包括四种：
 * - auto
 * - static
 * - extern：全局变量默认的
 * - register
 */
extern ngx_thread_volatile ngx_str_t  ngx_cached_err_log_time;
extern ngx_thread_volatile ngx_str_t  ngx_cached_http_time;
extern ngx_thread_volatile ngx_str_t  ngx_cached_http_log_time;

extern ngx_epoch_msec_t    ngx_start_msec;

/*
 * msecs elapsed since ngx_start_msec in the current event cycle,
 * used in ngx_event_add_timer() and ngx_event_find_timer()
 */
extern ngx_epoch_msec_t  ngx_elapsed_msec;

/*
 * msecs elapsed since ngx_start_msec in the previous event cycle,
 * used in ngx_event_expire_timers()
 */
extern ngx_epoch_msec_t  ngx_old_elapsed_msec;



#endif /* _NGX_TIMES_H_INCLUDED_ */
