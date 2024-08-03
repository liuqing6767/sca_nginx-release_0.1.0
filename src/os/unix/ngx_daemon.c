
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * 操作系统基础：进程管理
 *
 * 进程是正在运行中的程序，包含至少一个线程
 * 运行一个新的进程有两种方式：
 * - exec系统调用。它有一系列的函数，比如 int execl(const char *path, const char *arg, ...)。调用后不会返回，而是跳转到新的程序入口
 * - fork()系统调用。会创建一个子进程。调用成功后程序会继续进行。子进程中fork会返回0，父进程中会返回子进程的pid
 *
 * 新的进程一般都会丢失挂起信号、资源统计信息，文件锁也不会被继承
 */
int ngx_daemon(ngx_log_t *log)
{
    int  fd;

    switch (fork()) {
    case -1:
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "fork() failed");
        return NGX_ERROR;

    case 0:
        break;

    // 父进程退出
    default:
        exit(0);
    }

    ngx_pid = ngx_getpid();

    // setsid 创建新的会话，并与当前会话分离。也就是说不再受当前终端控制
    if (setsid() == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "setsid() failed");
        return NGX_ERROR;
    }

    umask(0);

    // 将 stdin 和 stdout 都丢弃
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "open(\"/dev/null\") failed");
        return NGX_ERROR;
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDIN) failed");
        return NGX_ERROR;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDOUT) failed");
        return NGX_ERROR;
    }

#if 0
    if (dup2(fd, STDERR_FILENO) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "dup2(STDERR) failed");
        return NGX_ERROR;
    }
#endif

    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "close() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}
