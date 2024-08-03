
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>


ngx_int_t  ngx_ncpu;
ngx_int_t  ngx_max_sockets;
ngx_int_t  ngx_inherited_nonblocking;


struct rlimit  rlmt;


#if (NGX_POSIX_IO)

ngx_os_io_t ngx_os_io = {
    ngx_unix_recv,
    ngx_readv_chain,
    NULL,
    ngx_writev_chain,
    0
};


int ngx_os_init(ngx_log_t *log)
{
    return ngx_posix_init(log);
}


#endif


void ngx_signal_handler(int signo);


typedef struct {
     int     signo;
     char   *signame;
     void  (*handler)(int signo);
} ngx_signal_t;


// signals 是所有的信号的登记
ngx_signal_t  signals[] = {
    { ngx_signal_value(NGX_RECONFIGURE_SIGNAL),
      "SIG" ngx_value(NGX_RECONFIGURE_SIGNAL),
      ngx_signal_handler },

    { ngx_signal_value(NGX_REOPEN_SIGNAL),
      "SIG" ngx_value(NGX_REOPEN_SIGNAL),
      ngx_signal_handler },

    { ngx_signal_value(NGX_NOACCEPT_SIGNAL),
      "SIG" ngx_value(NGX_NOACCEPT_SIGNAL),
      ngx_signal_handler },

    { ngx_signal_value(NGX_TERMINATE_SIGNAL),
      "SIG" ngx_value(NGX_TERMINATE_SIGNAL),
      ngx_signal_handler },

    { ngx_signal_value(NGX_SHUTDOWN_SIGNAL),
      "SIG" ngx_value(NGX_SHUTDOWN_SIGNAL),
      ngx_signal_handler },

    { ngx_signal_value(NGX_CHANGEBIN_SIGNAL),
      "SIG" ngx_value(NGX_CHANGEBIN_SIGNAL),
      ngx_signal_handler },

    { SIGALRM, "SIGALRM", ngx_signal_handler },

    { SIGINT, "SIGINT", ngx_signal_handler },

    { SIGIO, "SIGIO", ngx_signal_handler },

    { SIGCHLD, "SIGCHLD", ngx_signal_handler },

    { SIGPIPE, "SIGPIPE, SIG_IGN", SIG_IGN },

    { 0, NULL, NULL }
};

/*
 * ngx_posix_init 初始化操作系统。包括注册信号，确认资源使用限制
 *
 * 信号是一种单向异步通知机制
 * 信号有明确的生命周期，包括：产生信号，内核存储信号，内核处理信号
 * 
 * fork创建的子进程会继承父进程的所有信号处理；
 * exec创建的子进程会被设置为默认操作
 * 
 * C标准库定义了基本的信号操作：
 * - sighandler_t signal(int signo, sighandler_t  handler)
 * 
 * POSIX定义了高级信号管理：
 * - int sigaction(int signo, const struct sigaction *act, struct sigaction *oldact)
 */ 
ngx_int_t ngx_posix_init(ngx_log_t *log)
{
    ngx_signal_t      *sig;
    struct sigaction   sa;

    ngx_pagesize = getpagesize();

    if (ngx_ncpu == 0) {
        ngx_ncpu = 1;
    }

    // 进行信号注册
    for (sig = signals; sig->signo != 0; sig++) {
        ngx_memzero(&sa, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "sigaction(%s) failed", sig->signame);
            return NGX_ERROR;
        }
    }

    // 获取资源使用限制
    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, errno,
                      "getrlimit(RLIMIT_NOFILE) failed)");
        return NGX_ERROR;
    }

    ngx_max_sockets = rlmt.rlim_cur;

#if (HAVE_INHERITED_NONBLOCK)
    ngx_inherited_nonblocking = 1;
#else
    ngx_inherited_nonblocking = 0;
#endif

    return NGX_OK;
}


void ngx_posix_status(ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_INFO, log, 0,
                  "getrlimit(RLIMIT_NOFILE): " RLIM_T_FMT ":" RLIM_T_FMT,
                  rlmt.rlim_cur, rlmt.rlim_max);
}


// ngx_signal_handler 是 ngx处理信号的函数
void ngx_signal_handler(int signo)
{
    char            *action;
    struct timeval   tv;
    ngx_int_t        ignore;
    ngx_err_t        err;
    ngx_signal_t    *sig;

    ignore = 0;

    err = ngx_errno;

    for (sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }

    ngx_gettimeofday(&tv);
    ngx_time_update(tv.tv_sec);

    action = "";

    // 先确认当前进程的角色
    switch (ngx_process) {

    case NGX_PROCESS_MASTER:
    case NGX_PROCESS_SINGLE:
        switch (signo) {

        case ngx_signal_value(NGX_SHUTDOWN_SIGNAL):
            // 这里的设置，会在 ngx_*_process_cycle 中处理
            ngx_quit = 1;
            action = ", shutting down";
            break;

        case ngx_signal_value(NGX_TERMINATE_SIGNAL):
        case SIGINT:
            ngx_terminate = 1;
            action = ", exiting";
            break;

        case ngx_signal_value(NGX_NOACCEPT_SIGNAL):
            ngx_noaccept = 1;
            action = ", stop the accepting connections";
            break;

        case ngx_signal_value(NGX_RECONFIGURE_SIGNAL):
            ngx_reconfigure = 1;
            action = ", reconfiguring";
            break;

        case ngx_signal_value(NGX_REOPEN_SIGNAL):
            ngx_reopen = 1;
            action = ", reopen logs";
            break;

        case ngx_signal_value(NGX_CHANGEBIN_SIGNAL):
            if (getppid() > 1 || ngx_new_binary > 0) {

                /*
                 * Ignore the signal in the new binary if its parent is
                 * not the init process, i.e. the old binary's process
                 * is still running.  Or ingore the signal in the old binary's
                 * process if the new binary's process is already running.
                 */

                action = ", ignoring";
                ignore = 1;
                break;
            }

            ngx_change_binary = 1;
            action = ", changing binary";
            break;

        case SIGALRM:
            if (!ngx_terminate) {
                ngx_timer = 1;
                action = ", shutting down old worker processes";
            }

            break;

        case SIGIO:
            ngx_sigio = 1;
            break;

        case SIGCHLD:
            ngx_reap = 1;
            break;
        }

        break;

    case NGX_PROCESS_WORKER:
        switch (signo) {

        case ngx_signal_value(NGX_SHUTDOWN_SIGNAL):
            ngx_quit = 1;
            action = ", shutting down";
            break;

        case ngx_signal_value(NGX_TERMINATE_SIGNAL):
        case SIGINT:
            ngx_terminate = 1;
            action = ", exiting";
            break;

        case ngx_signal_value(NGX_REOPEN_SIGNAL):
            ngx_reopen = 1;
            action = ", reopen logs";
            break;

        case ngx_signal_value(NGX_RECONFIGURE_SIGNAL):
        case ngx_signal_value(NGX_NOACCEPT_SIGNAL):
        case ngx_signal_value(NGX_CHANGEBIN_SIGNAL):
        case SIGIO:
            action = ", ignoring";
            break;
        }

        break;
    }

    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0,
                  "signal %d (%s) received%s", signo, sig->signame, action);

    if (ignore) {
        ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0,
                      "the changing binary signal is ignored: "
                      "you should shutdown or terminate "
                      "before either old or new binary's process");
    }

    if (signo == SIGCHLD) {
        ngx_process_get_status();
    }

    ngx_set_errno(err);
}


int ngx_posix_post_conf_init(ngx_log_t *log)
{
    ngx_fd_t  pp[2];

    if (pipe(pp) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "pipe() failed");
        return NGX_ERROR;
    }

    if (dup2(pp[1], STDERR_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, errno, "dup2(STDERR) failed");
        return NGX_ERROR;
    }

    if (pp[1] > STDERR_FILENO) {
        if (close(pp[1]) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, errno, "close() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}
