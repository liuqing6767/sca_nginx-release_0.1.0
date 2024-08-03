
/*
 * Copyright (C) Igor Sysoev
 */


/*
 * 基础语法：#include
 * 可以简单的理解将文件内容粘贴到此处
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <nginx.h>


/*
 * 基本语法： static 关键字
 * 变量的作用域：
 * - 局部变量：默认在函数内部
 * - 全局变量：整个程序都可用
 * 
 * 变量/函数的生命周期：
 * - 局部变量：变量所在的局部范围
 * - 全局变量：默认是整个程序
 * 
 * 使用static关键字后：
 * - 局部变量：放大其生命周期，和进程一同存在
 * - 全局变量：限制其作用域为当前开始到文件结束，其他文件无法访问
 * - 函数：限制其作用域为当前开始到文件结束，其他文件无法访问
 */ 

static ngx_int_t ngx_add_inherited_sockets(ngx_cycle_t *cycle);
static ngx_int_t ngx_getopt(ngx_master_ctx_t *ctx, ngx_cycle_t *cycle);
static void *ngx_core_module_create_conf(ngx_cycle_t *cycle);
static char *ngx_core_module_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_set_user(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_core_commands[] = {

    { ngx_string("daemon"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_core_conf_t, daemon),
      NULL },

    { ngx_string("master_process"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_core_conf_t, master),
      NULL },

    { ngx_string("worker_processes"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_core_conf_t, worker_processes),
      NULL },

#if (NGX_THREADS)

    { ngx_string("worker_threads"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_core_conf_t, worker_threads),
      NULL },

    { ngx_string("thread_stack_size"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_core_conf_t, thread_stack_size),
      NULL },

#endif

    { ngx_string("user"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE12,
      ngx_set_user,
      0,
      0,
      NULL },

    { ngx_string("pid"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, pid),
      NULL },

      ngx_null_command
};



static ngx_core_module_t  ngx_core_module_ctx = {
    name: ngx_string("core"),
    create_conf: ngx_core_module_create_conf,
    init_conf: ngx_core_module_init_conf
};


ngx_module_t  ngx_core_module = {
    NGX_MODULE,
    &ngx_core_module_ctx,                  /* module context */
    ngx_core_commands,                     /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init module */
    NULL                                   /* init process */
};


ngx_uint_t  ngx_max_module;


/*
 * 1 程序的总入口
 */
int main(int argc, char *const *argv)
{
#if defined __FreeBSD__
    ngx_debug_init();
#endif

    // 这个变量没有什么用
    /* TODO */ ngx_max_sockets = -1;

    /*
     * 基本数据结构 1.1：时间
     * 初始化时间
     */
    ngx_time_init();

#if (HAVE_PCRE)
    ngx_regex_init();
#endif

    // 得到进程ID
    ngx_pid = ngx_getpid();

    // 基本数据结构 1.2：日志
    ngx_log_t         *log;
    // 初始化日志，输出到 stderr
    if (!(log = ngx_log_init_stderr())) {
        return 1;
    }

#if (NGX_OPENSSL)
    ngx_ssl_init(log);
#endif

    /* init_cycle->log is required for signal handlers and ngx_getopt() */

    // 启动参数。将启动参数保留下来
    ngx_master_ctx_t   ctx;
    ngx_memzero(&ctx, sizeof(ngx_master_ctx_t));
    ctx.argc = argc;
    ctx.argv = argv;

    // 初始化 周期(根据配置初始化上下文)
    ngx_cycle_t       init_cycle;
    ngx_memzero(&init_cycle, sizeof(ngx_cycle_t));
    init_cycle.log = log;
    ngx_cycle = &init_cycle;

    // 内存管理：申请 1kb的内存
    if (!(init_cycle.pool = ngx_create_pool(1024, log))) {
        return 1;
    }

    // 解析请求参数，本版本只有两个
    // -t: ngx_test_config == true
    // -c: 配置文件路径
    if (ngx_getopt(&ctx, &init_cycle) == NGX_ERROR) {
        return 1;
    }

    if (ngx_test_config) {
        log->log_level = NGX_LOG_INFO;
    }

    // 初始化操作系统、信号等
    if (ngx_os_init(log) == NGX_ERROR) {
        return 1;
    }

    // 初始化继承的socket（通过环境变量完成socket的继承）
    if (ngx_add_inherited_sockets(&init_cycle) == NGX_ERROR) {
        return 1;
    }

    // ngx_modules 是 ./configure 动态生成的
    // 下标需要重新初始化一下
    ngx_int_t          module_idx;
    ngx_max_module = 0;
    for (module_idx = 0; ngx_modules[module_idx]; module_idx++) {
        ngx_modules[module_idx]->index = ngx_max_module++;
    }

    // 初始化 核心变量
    ngx_cycle_t       *cycle;
    // 主流程核心：解析配置文件，初始化各个模块
    cycle = ngx_init_cycle(&init_cycle);
    if (cycle == NULL) {
        if (ngx_test_config) {
            ngx_log_error(NGX_LOG_EMERG, log, 0,
                          "the configuration file %s test failed",
                          init_cycle.conf_file.data);
        }

        return 1;
    }

    // -t 启动，只是校验配置的正确性。直接推出，不进入工作流程
    if (ngx_test_config) {
        ngx_log_error(NGX_LOG_INFO, log, 0,
                      "the configuration file %s was tested successfully",
                      init_cycle.conf_file.data);
        return 0;
    }

    // 记录一下操作系统的信息
    ngx_os_status(cycle->log);

    ngx_cycle = cycle;

    ngx_core_conf_t   *ccf;
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    // ccf = (ngx_core_conf_t *) cycle->conf_ctx[ngx_core_module.index];

    /*
     * NGX启动有两个维度，四种方式
     *          前台    后台
     *  单进程
     *  多进程
     */

    ngx_process = ccf->master ? NGX_PROCESS_MASTER : NGX_PROCESS_SINGLE;

#if (WIN32)

#if 0

    TODO:

    if (ccf->run_as_service) {
        if (ngx_service(cycle->log) == NGX_ERROR) {
            return 1;
        }

        return 0;
    }
#endif

#else

    if (!ngx_inherited && ccf->daemon) {
        // 退出父进程，fork出子进程。子进程继续执行
        if (ngx_daemon(cycle->log) == NGX_ERROR) {
            return 1;
        }

        ngx_daemonized = 1;
    }

    // 创建pidfile。不重要，跳过
    if (ngx_create_pidfile(cycle, NULL) == NGX_ERROR) {
        return 1;
    }

#endif

    if (ngx_process == NGX_PROCESS_MASTER) {
        // 主进程开始工作，会派生工作进程
        ngx_master_process_cycle(cycle, &ctx);

    } else {
        // 单个进程开始工作
        // 同时负责master和worker进程相关的工作
        ngx_single_process_cycle(cycle, &ctx);
    }

    return 0;
}


// ngx_add_inherited_sockets 用来从环境变量中继承 socket
static ngx_int_t ngx_add_inherited_sockets(ngx_cycle_t *cycle)
{
    u_char              *p, *v, *inherited;
    ngx_socket_t         s;
    ngx_listening_t     *ls;

    // TODO 在哪里setenv的？..
    // 平滑升级的时候才会设置，内容为 socket 列表，中间用 : / ; 分开
    inherited = (u_char *) getenv(NGINX_VAR);

    if (inherited == NULL) { // 环境变量要是没有这个值，直接返回即可
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                  "using inherited sockets from \"%s\"", inherited);

    // 在内存池中初始化监听列表数组
    ngx_init_array(cycle->listening, cycle->pool,
                   10, sizeof(ngx_listening_t), NGX_ERROR);
    
    /* 等价于：
    // ngx_test_null(cycle->listening.elts, ngx_palloc(cycle->pool, 10 * sizeof(ngx_listening_t)), NGX_ERROR);
    if((cycle->listening.elts = ngx_palloc(cycle->pool, 10 * sizeof(ngx_listening_t))) == NULL) {
        return NGX_ERROR;
    }
    cycle->listening.nelts = 0;
    cycle->listening.size = 10 * sizeof(ngx_listening_t);
    cycle->listening.nalloc = 10;
    cycle->listening.pool = cycle->pool;
    */

    for (p = inherited, v = p; *p; p++) {
        if (*p == ':' || *p == ';') {
            // 用 : 或者  ;  切割环境变量，得到 继承的socket 
            s = ngx_atoi(v, p - v);
            if (s == NGX_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                              "invalid socket number \"%s\" in "
                              NGINX_VAR " enviroment variable, "
                              "ignoring the rest of the variable", v);
                break;
            }

            v = p + 1;

            // 将 socket放入监听列表
            if (!(ls = ngx_push_array(&cycle->listening))) {
                return NGX_ERROR;
            }

            ls->fd = s;
        }
    }

    ngx_inherited = 1;

    // 初始化监听数组的数据
    return ngx_set_inherited_sockets(cycle);
}

// ngx_exec_new_binary 平滑升级的方案
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv)
{
    char             *env[2], *var, *p;
    ngx_uint_t        i;
    ngx_pid_t         pid;
    ngx_exec_ctx_t    ctx;
    ngx_listening_t  *ls;

    ctx.path = argv[0];
    ctx.name = "new binary process";
    ctx.argv = argv;

    var = ngx_alloc(sizeof(NGINX_VAR)
                            + cycle->listening.nelts * (NGX_INT32_LEN + 1) + 2,
                    cycle->log);

    p = (char *) ngx_cpymem(var, NGINX_VAR "=", sizeof(NGINX_VAR));

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {
        p += ngx_snprintf(p, NGX_INT32_LEN + 2, "%u;", ls[i].fd);
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, cycle->log, 0, "inherited: %s", var);

    env[0] = var;
    env[1] = NULL;
    ctx.envp = (char *const *) &env;

    // 启动master进程
    pid = ngx_execute(cycle, &ctx);

    ngx_free(var);

    return pid;
}

/* ngx_getopt 从请求参数里面得到选项
 * 比如启动命令为: /sbin/nginx -t -c /sbin/config/nginx.conf 会解析为 
 * 使用 配置 /sbin/config/nginx.conf 进行测试
 * 
 * 当前版本只接收 -t 和 -c
 */
static ngx_int_t ngx_getopt(ngx_master_ctx_t *ctx, ngx_cycle_t *cycle)
{
    ngx_int_t  i;

    for (i = 1; i < ctx->argc; i++) {
        if (ctx->argv[i][0] != '-') {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "invalid option: \"%s\"", ctx->argv[i]);
            return NGX_ERROR;
        }

        switch (ctx->argv[i][1]) {

        case 't':
            ngx_test_config = 1;
            break;

        case 'c':
            if (ctx->argv[i + 1] == NULL) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                              "the option: \"%s\" requires file name",
                              ctx->argv[i]);
                return NGX_ERROR;
            }

            cycle->conf_file.data = (u_char *) ctx->argv[++i];
            cycle->conf_file.len = ngx_strlen(cycle->conf_file.data);
            break;

        default:
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "invalid option: \"%s\"", ctx->argv[i]);
            return NGX_ERROR;
        }
    }

    // 如果配置文件没有设置，使用默认的配置文件路径
    if (cycle->conf_file.data == NULL) {
        cycle->conf_file.len = sizeof(NGX_CONF_PATH) - 1;
        cycle->conf_file.data = (u_char *) NGX_CONF_PATH;
    }

    if (ngx_conf_full_name(cycle, &cycle->conf_file) == NGX_ERROR) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void *ngx_core_module_create_conf(ngx_cycle_t *cycle)
{
    ngx_core_conf_t  *ccf;

    if (!(ccf = ngx_pcalloc(cycle->pool, sizeof(ngx_core_conf_t)))) {
        return NULL;
    }
    /* set by pcalloc()
     *
     * ccf->pid = NULL;
     * ccf->newpid = NULL;
     */
    ccf->daemon = NGX_CONF_UNSET;
    ccf->master = NGX_CONF_UNSET;
    ccf->worker_processes = NGX_CONF_UNSET;
#if (NGX_THREADS)
    ccf->worker_threads = NGX_CONF_UNSET;
    ccf->thread_stack_size = NGX_CONF_UNSET;
#endif
    ccf->user = (ngx_uid_t) NGX_CONF_UNSET;
    ccf->group = (ngx_gid_t) NGX_CONF_UNSET;

    return ccf;
}


static char *ngx_core_module_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

#if !(WIN32)
    struct passwd    *pwd;
    struct group     *grp;
#endif

    ngx_conf_init_value(ccf->daemon, 1);
    ngx_conf_init_value(ccf->master, 1);
    ngx_conf_init_value(ccf->worker_processes, 1);

#if (NGX_THREADS)
    ngx_conf_init_value(ccf->worker_threads, 0);
    ngx_threads_n = ccf->worker_threads;
    ngx_conf_init_size_value(ccf->thread_stack_size, 2 * 1024 * 1024);
#endif

#if !(WIN32)

    if (ccf->user == (uid_t) NGX_CONF_UNSET) {

        pwd = getpwnam("nobody");
        if (pwd == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "getpwnam(\"nobody\") failed");
            return NGX_CONF_ERROR;
        }

        ccf->user = pwd->pw_uid;

        grp = getgrnam("nobody");
        if (grp == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "getgrnam(\"nobody\") failed");
            return NGX_CONF_ERROR;
        }

        ccf->group = grp->gr_gid;
    }

    if (ccf->pid.len == 0) {
        ccf->pid.len = sizeof(NGX_PID_PATH) - 1;
        ccf->pid.data = NGX_PID_PATH;
    }

    if (ngx_conf_full_name(cycle, &ccf->pid) == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    ccf->newpid.len = ccf->pid.len + sizeof(NGX_NEWPID_EXT);

    if (!(ccf->newpid.data = ngx_palloc(cycle->pool, ccf->newpid.len))) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(ngx_cpymem(ccf->newpid.data, ccf->pid.data, ccf->pid.len),
               NGX_NEWPID_EXT, sizeof(NGX_NEWPID_EXT));

#endif

    return NGX_CONF_OK;
}


static char *ngx_set_user(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (WIN32)

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"user\" is not supported, ignored");

    return NGX_CONF_OK;

#else

    ngx_core_conf_t  *ccf = conf;

    struct passwd    *pwd;
    struct group     *grp;
    ngx_str_t        *value;

    if (ccf->user != (uid_t) NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = (ngx_str_t *) cf->args->elts;

    pwd = getpwnam((const char *) value[1].data);
    if (pwd == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "getpwnam(%s) failed", value[1].data);
        return NGX_CONF_ERROR;
    }

    ccf->user = pwd->pw_uid;

    if (cf->args->nelts == 2) {
        return NGX_CONF_OK;
    }

    grp = getgrnam((const char *) value[2].data);
    if (grp == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "getgrnam(%s) failed", value[1].data);
        return NGX_CONF_ERROR;
    }

    ccf->group = grp->gr_gid;

    return NGX_CONF_OK;

#endif
}
