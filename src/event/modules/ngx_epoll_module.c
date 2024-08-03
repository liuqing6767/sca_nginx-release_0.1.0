
/*
 * Copyright (C) Igor Sysoev
 */

/*
 * epoll 是 Linux提供的高级 I/O 多路复用的方案。
 * select 和 poll 每次调用都需要所有被监听的文件描述符列表，内核必须遍历所有被监视的文件描述符
 * 
 * epoll 将监听注册从实际监听中分离出来，从而解决效率问题。
 * 逻辑变成：
 * 1. 初始化 epoll 上下文  
 * 2. 从上下文中加入/删除监视的文件描述符
 * 3. 执行正在的事件等待（event wait）
 * 
 * 1. int epfd = epoll_create1(int flags)
 * 2. ret epoll_ctl(
 *      int epfd,
 *      int op, // add/del/mod
 *      int fd,
 *      struct epoll_event *event,
 * )
 * 3. events = epoll_wait(
 *      int epfd,
 *      struct epoll_event *event,
 *      int maxevents,      // 最多可以有的事件
 *      int timeout         // 最多等待时长
 * )
 * 
 * epoll 的 epoll_ctl 参数中的 event 中的 events项配置有两个选择：
 * - Level-triggerd。条件触发是默认的行为，poll 和  select 也是这种。表示有状态发生就触发。
 * - Edge-triggered。边缘触发需要特殊的编程解决方案。表示有状态改变就触发。
 * 举例：当第一次有数据写入，两者都触发；当有新的数据到来，只有条件触发才触发。
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#if (TEST_BUILD_EPOLL)

/* epoll declarations */

#define EPOLLIN        0x001
#define EPOLLPRI       0x002
#define EPOLLOUT       0x004
#define EPOLLRDNORM    0x040
#define EPOLLRDBAND    0x080
#define EPOLLWRNORM    0x100
#define EPOLLWRBAND    0x200
#define EPOLLMSG       0x400
#define EPOLLERR       0x008
#define EPOLLHUP       0x010

#define EPOLLET        0x80000000
#define EPOLLONESHOT   0x40000000

#define EPOLL_CTL_ADD  1
#define EPOLL_CTL_DEL  2
#define EPOLL_CTL_MOD  3

typedef union epoll_data {
    void         *ptr;
    int           fd;
    uint32_t      u32;
    uint64_t      u64;
} epoll_data_t;

struct epoll_event {
    uint32_t      events;
    epoll_data_t  data;
};

int epoll_create(int size);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int nevents, int timeout);

int epoll_create(int size)
{
    return -1;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    return -1;
}

int epoll_wait(int epfd, struct epoll_event *events, int nevents, int timeout)
{
    return -1;
}

#endif


typedef struct {
    u_int  events;
} ngx_epoll_conf_t;


static int ngx_epoll_init(ngx_cycle_t *cycle);
static void ngx_epoll_done(ngx_cycle_t *cycle);
static int ngx_epoll_add_event(ngx_event_t *ev, int event, u_int flags);
static int ngx_epoll_del_event(ngx_event_t *ev, int event, u_int flags);
static int ngx_epoll_add_connection(ngx_connection_t *c);
static int ngx_epoll_del_connection(ngx_connection_t *c, u_int flags);
static int ngx_epoll_process_events(ngx_cycle_t *cycle);

static void *ngx_epoll_create_conf(ngx_cycle_t *cycle);
static char *ngx_epoll_init_conf(ngx_cycle_t *cycle, void *conf);

static int                  ep = -1;
// 当前worker进程的事件列表
static struct epoll_event  *event_list;
// number of envents
static u_int                nevents;


static ngx_str_t      epoll_name = ngx_string("epoll");

static ngx_command_t  ngx_epoll_commands[] = {

    {ngx_string("epoll_events"),
     NGX_EVENT_CONF|NGX_CONF_TAKE1,
     ngx_conf_set_num_slot,
     0,
     offsetof(ngx_epoll_conf_t, events),
     NULL},

    ngx_null_command
};


ngx_event_module_t  ngx_epoll_module_ctx = {
    &epoll_name,
    ngx_epoll_create_conf,               /* create configuration */
    ngx_epoll_init_conf,                 /* init configuration */

    {
        ngx_epoll_add_event,             /* add an event */
        ngx_epoll_del_event,             /* delete an event */
        ngx_epoll_add_event,             /* enable an event */
        ngx_epoll_del_event,             /* disable an event */
        ngx_epoll_add_connection,        /* add an connection */
        ngx_epoll_del_connection,        /* delete an connection */
        NULL,                            /* process the changes */
        ngx_epoll_process_events,        /* process the events */
        ngx_epoll_init,                  /* init the events */
        ngx_epoll_done,                  /* done the events */
    }
};

ngx_module_t  ngx_epoll_module = {
    NGX_MODULE,
    &ngx_epoll_module_ctx,               /* module context */
    ngx_epoll_commands,                  /* module directives */
    NGX_EVENT_MODULE,                    /* module type */
    NULL,                                /* init module */
    NULL                                 /* init process */
};


static int ngx_epoll_init(ngx_cycle_t *cycle)
{
    size_t             n;
    ngx_event_conf_t  *ecf;
    ngx_epoll_conf_t  *epcf;

    ecf = ngx_event_get_conf(cycle->conf_ctx, ngx_event_core_module);

    epcf = ngx_event_get_conf(cycle->conf_ctx, ngx_epoll_module);

    if (ep == -1) {
        ep = epoll_create(ecf->connections / 2);

        if (ep == -1) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "epoll_create() failed");
            return NGX_ERROR;
        }
    }

    if (nevents < epcf->events) { // 可能是热加载
        if (event_list) {
            ngx_free(event_list);
        }

        // 一次性申请内存
        event_list = ngx_alloc(sizeof(struct epoll_event) * epcf->events,
                               cycle->log);
        if (event_list == NULL) {
            return NGX_ERROR;
        }
    }

    nevents = epcf->events;

    ngx_io = ngx_os_io;

    ngx_event_actions = ngx_epoll_module_ctx.actions;

#if (HAVE_CLEAR_EVENT)
    ngx_event_flags = NGX_USE_CLEAR_EVENT
#else
    ngx_event_flags = NGX_USE_LEVEL_EVENT
#endif
                      |NGX_HAVE_GREEDY_EVENT
                      |NGX_USE_EPOLL_EVENT;

    return NGX_OK;
}


static void ngx_epoll_done(ngx_cycle_t *cycle)
{
    if (close(ep) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "epoll close() failed");
    }

    ep = -1;

    ngx_free(event_list);

    event_list = NULL;
    nevents = 0;
}


/* 添加事件，实现为调用 epoll_ctl 调整 epoll 上下文
 * 会在master创建worker进程时调用，用来添加通信用的 sockerpair
 * 还会在 accept 时调用
 */
static int ngx_epoll_add_event(ngx_event_t *ev, int event, u_int flags)
{
    int                  op, prev;
    ngx_event_t         *e;
    ngx_connection_t    *c;
    struct epoll_event   ee;

    c = ev->data;

    if (event == NGX_READ_EVENT) {
        e = c->write;
        prev = EPOLLOUT;
#if (NGX_READ_EVENT != EPOLLIN)
        event = EPOLLIN;
#endif

    } else {
        e = c->read;
        prev = EPOLLIN;
#if (NGX_WRITE_EVENT != EPOLLOUT)
        event = EPOLLOUT;
#endif
    }

    if (e->active) {
        op = EPOLL_CTL_MOD;
        event |= prev;

    } else {
        op = EPOLL_CTL_ADD;
    }

    ee.events = event | flags;
    ee.data.ptr = (void *) ((uintptr_t) c | ev->instance);

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "epoll add event: fd:%d op:%d ev:%08X",
                   c->fd, op, ee.events);

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_errno,
                      "epoll_ctl(%d, %d) failed", op, c->fd);
        return NGX_ERROR;
    }

    ev->active = 1;
#if 0
    ev->oneshot = (flags & NGX_ONESHOT_EVENT) ? 1 : 0;
#endif

    return NGX_OK;
}


static int ngx_epoll_del_event(ngx_event_t *ev, int event, u_int flags)
{
    int                  op, prev;
    ngx_event_t         *e;
    ngx_connection_t    *c;
    struct epoll_event   ee;

    /*
     * when the file descriptor is closed the epoll automatically deletes
     * it from its queue so we do not need to delete explicity the event
     * before the closing the file descriptor
     */

    if (flags & NGX_CLOSE_EVENT) {
        ev->active = 0;
        return NGX_OK;
    }

    c = ev->data;

    if (event == NGX_READ_EVENT) {
        e = c->write;
        prev = EPOLLOUT;

    } else {
        e = c->read;
        prev = EPOLLIN;
    }

    if (e->active) {
        op = EPOLL_CTL_MOD;
        ee.events = prev | flags;
        ee.data.ptr = (void *) ((uintptr_t) c | ev->instance);

    } else {
        op = EPOLL_CTL_DEL;
        ee.events = 0;
        ee.data.ptr = NULL;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "epoll del event: fd:%d op:%d ev:%08X",
                   c->fd, op, ee.events);

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_errno,
                      "epoll_ctl(%d, %d) failed", op, c->fd);
        return NGX_ERROR;
    }

    ev->active = 0;

    return NGX_OK;
}


static int ngx_epoll_add_connection(ngx_connection_t *c)
{
    struct epoll_event  ee;

    ee.events = EPOLLIN|EPOLLOUT|EPOLLET;
    ee.data.ptr = (void *) ((uintptr_t) c | c->read->instance);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "epoll add connection: fd:%d ev:%08X", c->fd, ee.events);

    if (epoll_ctl(ep, EPOLL_CTL_ADD, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      "epoll_ctl(EPOLL_CTL_ADD, %d) failed", c->fd);
        return NGX_ERROR;
    }

    c->read->active = 1;
    c->write->active = 1;

    return NGX_OK;
}


static int ngx_epoll_del_connection(ngx_connection_t *c, u_int flags)
{
    int                  op;
    struct epoll_event   ee;

    /*
     * when the file descriptor is closed the epoll automatically deletes
     * it from its queue so we do not need to delete explicity the event
     * before the closing the file descriptor
     */

    if (flags & NGX_CLOSE_EVENT) {
        c->read->active = 0;
        c->write->active = 0;
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "epoll del connection: fd:%d", c->fd);

    op = EPOLL_CTL_DEL;
    ee.events = 0;
    ee.data.ptr = NULL;

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      "epoll_ctl(%d, %d) failed", op, c->fd);
        return NGX_ERROR;
    }

    c->read->active = 0;
    c->write->active = 0;

    return NGX_OK;
}


// 核心逻辑：事件处理
// epoll_wait 会阻塞，直到有新的HTTP请求到来
int ngx_epoll_process_events(ngx_cycle_t *cycle)
{
    int                events;
    size_t             n;
    ngx_int_t          instance, i;
    ngx_uint_t         lock, accept_lock, expire;
    ngx_err_t          err;
    ngx_log_t         *log;
    ngx_msec_t         timer;
    ngx_event_t       *rev, *wev;
    struct timeval     tv;
    ngx_connection_t  *c;
    ngx_epoch_msec_t   delta;

    for ( ;; ) { 
        timer = ngx_event_find_timer();

#if (NGX_THREADS)

        if (timer == NGX_TIMER_ERROR) {
            return NGX_ERROR;
        }

        if (timer == NGX_TIMER_INFINITE || timer > 500) {
            timer = 500;
            break;
        }

#endif

        // 有定时器到期了
        if (timer != 0) { 
            break;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                       "epoll expired timer");

        ngx_event_expire_timers((ngx_msec_t)
                                    (ngx_elapsed_msec - ngx_old_elapsed_msec));

        if (ngx_posted_events && ngx_threaded) {
            ngx_wakeup_worker_thread(cycle);
        }
    }

    /* NGX_TIMER_INFINITE == INFTIM */

    if (timer == NGX_TIMER_INFINITE) {
        expire = 0;

    } else {
        expire = 1;
    }

    ngx_old_elapsed_msec = ngx_elapsed_msec;
    accept_lock = 0;

    /*
     * 惊群处理
     * nginx 有两个地方会有惊群。
     * 这里是第一个：有可读事件就绪
     * 第二个是：可读事件触发，执行回调
     * 
     * 解决方案：
     * 1 listen_fd 在加入 epoll之前，所有的worker进行抢锁，抢到锁的进程将 listen_fd 进入 epoll
     *      ngx_accept_mutex 就是这个原理。
     * 2 Linux内核解决惊群问题。NGNIX在 1.9.1 版本后开始支持 reuseport 关键字 `listen 80 reuseport`，
     *      内核会有策略（随机）选择处理的进程
     */
    if (ngx_accept_mutex) { // 开启了抢锁
        if (ngx_accept_disabled > 0) { // 当前worker进程负载过高，不再接收新连接
            ngx_accept_disabled--;

        } else {
            if (ngx_trylock_accept_mutex(cycle) == NGX_ERROR) { // 添加连接或者时间失败
                return NGX_ERROR;
            }

            if (ngx_accept_mutex_held) { // 抢锁成功
                accept_lock = 1;

            } else if (timer == NGX_TIMER_INFINITE  // 没抢到
                        || timer > ngx_accept_mutex_delay)
            {
                timer = ngx_accept_mutex_delay; // 更新时间， epoll_wait 进行等待
                expire = 0;
            }
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "epoll timer: %d", timer);

    events = epoll_wait(ep, event_list, nevents, timer);

    if (events == -1) { // 出错
        err = ngx_errno;
    } else {
        err = 0;
    }

    ngx_gettimeofday(&tv);
    ngx_time_update(tv.tv_sec);

    delta = ngx_elapsed_msec;
    ngx_elapsed_msec = (ngx_epoch_msec_t) tv.tv_sec * 1000
                                          + tv.tv_usec / 1000 - ngx_start_msec;

    if (timer != NGX_TIMER_INFINITE) {
        delta = ngx_elapsed_msec - delta; // 更新后的运行时间与上一次的差值

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                       "epoll timer: %d, delta: %d", timer, (int) delta);
    } else {
        if (events == 0) { // 永久阻塞的情形下还没有事件，直接退出
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "epoll_wait() returned no events without timeout");
            ngx_accept_mutex_unlock();
            return NGX_ERROR;
        }
    }

    if (err) {
        ngx_log_error((err == NGX_EINTR) ? NGX_LOG_INFO : NGX_LOG_ALERT,
                      cycle->log, err, "epoll_wait() failed");
        ngx_accept_mutex_unlock();
        return NGX_ERROR;
    }

    if (events > 0) {
        if (ngx_mutex_lock(ngx_posted_events_mutex) == NGX_ERROR) { // 获取  ngx_posted_events_mutex 失败
            ngx_accept_mutex_unlock();
            return NGX_ERROR;
        } 

        lock = 1;

    } else {
        lock =0;
    }

    log = cycle->log;

    for (i = 0; i < events; i++) { // 处理新来的事件
        c = event_list[i].data.ptr;

        instance = (uintptr_t) c & 1;
        c = (ngx_connection_t *) ((uintptr_t) c & (uintptr_t) ~1);

        // 处理读事件
        rev = c->read;

        if (c->fd == -1 || rev->instance != instance) {
            // c->fd == -1 表明当前连接已被释放，本事件为过期时间

            /*
             * the stale event from a file descriptor
             * that was just closed in this iteration
             */

            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                           "epoll: stale event " PTR_FMT, c);
            continue;
        }

#if (NGX_DEBUG0)
        log = c->log ? c->log : cycle->log;
#endif

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, log, 0,
                       "epoll: fd:%d ev:%04X d:" PTR_FMT,
                       c->fd, event_list[i].events, event_list[i].data);

        if (event_list[i].events & (EPOLLERR|EPOLLHUP)) {
            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, log, 0,
                           "epoll_wait() error on fd:%d ev:%04X",
                           c->fd, event_list[i].events);
        }

        if (event_list[i].events & ~(EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP)) {
            ngx_log_error(NGX_LOG_ALERT, log, 0,
                          "strange epoll_wait() events fd:%d ev:%04X",
                          c->fd, event_list[i].events);
        }

        // 处理写事件
        wev = c->write;

        if ((event_list[i].events & (EPOLLOUT|EPOLLERR|EPOLLHUP)) && wev->active) { // 写事件ready
            if (ngx_threaded) {
                wev->posted_ready = 1;
                ngx_post_event(wev);

            } else {
                wev->ready = 1;

                if (!ngx_accept_mutex_held) { // 没有锁的情况下直接处理
                    wev->event_handler(wev);

                } else { // 得到锁的情况下，放入队列。避免吞吐率降低
                    ngx_post_event(wev);
                }
            }
        }

        /*
         * EPOLLIN must be handled after EPOLLOUT because we use
         * the optimization to avoid the unnecessary mutex locking/unlocking
         * if the accept event is the last one.
         */

        if ((event_list[i].events & (EPOLLIN|EPOLLERR|EPOLLHUP)) && rev->active) { // 读事件ready
            if (ngx_threaded && !rev->accept) {
                rev->posted_ready = 1;

                ngx_post_event(rev);

                continue;
            }

            rev->ready = 1;

            if (!ngx_threaded && !ngx_accept_mutex_held) {
                rev->event_handler(rev);

            } else if (!rev->accept) {
                ngx_post_event(rev);

            } else if (ngx_accept_disabled <= 0) {

                ngx_mutex_unlock(ngx_posted_events_mutex);

                rev->event_handler(rev);

                if (ngx_accept_disabled > 0) {
                    ngx_accept_mutex_unlock();
                    accept_lock = 0;
                }

                if (i + 1 == events) { // 最后一个待处理的事件
                    lock = 0;
                    break;
                }

                if (ngx_mutex_lock(ngx_posted_events_mutex) == NGX_ERROR) {
                    if (accept_lock) {
                        ngx_accept_mutex_unlock();
                    }
                    return NGX_ERROR;
                }
            }
        }
    }
    /* 事件处理完成 */

    if (accept_lock) {
        ngx_accept_mutex_unlock();
    }

    if (lock) {
        ngx_mutex_unlock(ngx_posted_events_mutex);
    }

    // io 处理完成后都会执行时间事件处理函数，处理过期、超时等问题
    if (expire && delta) {
        ngx_event_expire_timers((ngx_msec_t) delta);
    }

    // 处理队列中的事件
    if (ngx_posted_events) {
        if (ngx_threaded) {
            ngx_wakeup_worker_thread(cycle);

        } else {
            ngx_event_process_posted(cycle);
        }
    }

    return NGX_OK;
}


static void *ngx_epoll_create_conf(ngx_cycle_t *cycle)
{
    ngx_epoll_conf_t  *epcf;

    ngx_test_null(epcf, ngx_palloc(cycle->pool, sizeof(ngx_epoll_conf_t)),
                  NGX_CONF_ERROR);

    epcf->events = NGX_CONF_UNSET;

    return epcf;
}


static char *ngx_epoll_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_epoll_conf_t *epcf = conf;

    ngx_conf_init_unsigned_value(epcf->events, 512);

    return NGX_CONF_OK;
}
