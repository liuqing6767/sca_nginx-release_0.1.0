Nginx 是最优秀的负载均衡器之一，通过学习其源码可以了解很多优秀的设计思想和实现细节。

# 学习重点

- 转发模型
- 事件机制
- 高效的数据结构和算法
- 非重点：协议解析等


# 通过阅读早期源码进行快速学习

选择最早的发布版本 (release-0.1.0)[https://github.com/nginx/nginx/tree/release-0.1.0] 进行分析。

通过阅读早期版本的源码了解核心设计是一个明智的选择：
- 学习目标是了解转发模型、核心流程
- Nginx 的核心框架在早期版本后应该就没有大的变动了，后续除了添加部分新的协议，就是更多的扩展模块和参数支持
- 早期代码行数不多，大概3W行。最小的代码已经16W行了

```
cloc  src/
     173 text files.
     173 unique files.                                          
       0 files ignored.

github.com/AlDanial/cloc v 1.90  T=0.26 s (654.0 files/s, 177900.0 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C                              105          10503           1684          28838
C/C++ Header                    67           1981            541           3440
Assembly                         1             18             10             47
-------------------------------------------------------------------------------
SUM:                           173          12502           2235          32325
-------------------------------------------------------------------------------
```

# 代码结构和核心数据结构

- auto： 自动生成的一些代码
- src：源代码所在地
    - core：基本数据结构和函数：string array log pool 等等
    - event：事件核心
        - modules：事件通知模块：epoll kqueue select 等等
    - os：平台特定代码
        - unix win32
    - http：核心的 HTTP 公共代码
    - imap：邮件模块

参考资料：(官方开发指南)[https://nginx.org/en/docs/dev/development_guide.html]

数据结构有：
- 数字： ngx_int_t ngx_uint_t
- 字符串
- 正则表达式
- 时间
- 容器：`Array` `Link List` `Queue` `RedBlack tree` `hash`
- 内存管理： `Heap` `Pool` `Shared memory`
- logging
- `cycle`：nginx运行时上下文
- `buffer`： 对 I/O操作的优化
- 网络：`connection`
- 事件： `i/o event` `timer event` `post event`

# 核心流程

```
user  nobody;
worker_processes  3;

#error_log  logs/error.log;
#pid        logs/nginx.pid;


events {
    connections  1024;
    use epoll;
}


http {
    include       conf/mime.types;
    default_type  application/octet-stream;

    sendfile  on;

    #gzip  on;

    server {
        listen  80;

        charset         on;
        source_charset  koi8-r;

        #access_log  logs/access.log;

        location / {
            root   html;
            index  index.html index.htm;
        }

    }

}
```

上述示例：
1. 进程机制：
    1. master进程启动后会用 nobody 用户创建 3个 worker进程
    1. master进程会监听信号事件，管理自身/worker进程
    1. master进程会必要时给worker进程发送消息
    1. master 会监听80端口的文件事件
1. 事件模块：每个woker进程都会使用最多服务1024个连接，使用epoll网络模型
    1. worker进程会监听信号事件
    1. worker进程会接收master的消息
    1. worker进程会监听文件事件（网络请求）
    1. worker进程会监听时间信号，处理连接超时
1. http模块：每个worker都会(抢锁后) accept 连接  提供 http 代理服务
    1. http 模块中有很多具体的模块，它们都有自己的phase回调点处理各种行为
    1. upstream 模块比较特殊，会和upstream建立连接，实现代理


# 总结
选择最早的release也有弊端，主要是功能比较简单：
1. 协议更少，比如不支持 http2， http3。stream 也没有
1. 模块更少，phase更少
1. 实现更简单：upstream 没有连接池，没有CPU亲和

但整体上是了解了NGX的流程了：
- 进程机制：master-worker 通信机制
- 事件机制：多事件模型可选择，事件触发
- http请求处理机制：phase， upstream


## nginx好的地方
- 性能高效，包括内存池的使用，io多路复用，红黑树维护的定时器
- 模块化设计，可以很容易的扩展

## nginx不好的地方
- 配置解析和业务耦合太严重。其实可以定义好配置的格式，一次性解析到配置对象，再初始化转发业务需要的对象会更好
- HTTP 的模块可以是个接口。大家也不用在初始化时候去注册phase，只需要请求过来时直接调用就行（初始化逻辑和业务逻辑解耦）

## 后续
- 按需看更新的版本的实现
- 按需进一步的抠细节
    - 信号量
    - 事件标志
    - 红黑树实现
    - 扩展模块实现
    - 共享内存