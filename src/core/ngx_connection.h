
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_listening_s  ngx_listening_t;

struct ngx_listening_s {
    ngx_socket_t        fd;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;    /* size of sockaddr */
    size_t              addr_text_max_len;/*存储IP地址的字符串最大长度*/
    ngx_str_t           addr_text;/*以字符串的形式存储IP地址*/

    int                 type;/*套接字类型，SOCK_STREAM表示TCP*/

    int                 backlog;//backlog队列长度
    int                 rcvbuf;//内核接收缓冲区大小
    int                 sndbuf;//内核发送缓冲区大小
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                 keepidle;
    int                 keepintvl;
    int                 keepcnt;
#endif

    /* handler of accepted connection */
	/*typedef void (*ngx_connection_handler_pt)(ngx_connection_t *c);*/
    ngx_connection_handler_pt   handler;/*新的TCP连接建立成功后的回调函数*/

    void               *servers;  /* array of ngx_http_in_addr_t, for example */

    ngx_log_t           log;
    ngx_log_t          *logp;

    size_t              pool_size;//为新的TCP连接创建内存池时，内存池初始大小
    /* should be here because of the AcceptEx() preread */
    size_t              post_accept_buffer_size;
    /* should be here because of the deferred accept */
    ngx_msec_t          post_accept_timeout;

    ngx_listening_t    *previous;//多个ngx_listening_t对象由previous指针组成单链表
    ngx_connection_t   *connection;

    unsigned            open:1;/*为1时表示当前监听套接字有效，为0表示正常关闭*/
    unsigned            remain:1;
    unsigned            ignore:1;

    unsigned            bound:1;       /* already bound 是否已绑定 */
    unsigned            inherited:1;   /* inherited from previous process */
    unsigned            nonblocking_accept:1;
    unsigned            listen:1;//是否已监听
    unsigned            nonblocking:1;
    unsigned            shared:1;    /* shared between threads or processes */
    unsigned            addr_ntop:1;

#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    unsigned            ipv6only:2;
#endif
    unsigned            keepalive:2;

#if (NGX_HAVE_DEFERRED_ACCEPT)
    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#ifdef SO_ACCEPTFILTER
    char               *accept_filter;
#endif
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;
#endif

};


typedef enum {
     NGX_ERROR_ALERT = 0,
     NGX_ERROR_ERR,
     NGX_ERROR_INFO,
     NGX_ERROR_IGNORE_ECONNRESET,
     NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;


typedef enum {
     NGX_TCP_NODELAY_UNSET = 0,
     NGX_TCP_NODELAY_SET,
     NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;


typedef enum {
     NGX_TCP_NOPUSH_UNSET = 0,
     NGX_TCP_NOPUSH_SET,
     NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;


#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01

//与客户端建立的连接（被动连接）
struct ngx_connection_s {
    void               *data;/*连接未使用时，用于充当空闲连接池中的next指针。当连接使用时，具体意义由模块决定*/
    ngx_event_t        *read;//读事件
    ngx_event_t        *write;//写事件

    ngx_socket_t        fd;//文件描述符

    ngx_recv_pt         recv;//直接接收数据的方法
    ngx_send_pt         send;//直接发送数据的方法
    ngx_recv_chain_pt   recv_chain;//以ngx_chain_t链表为参数来接收数据的方法
    ngx_send_chain_pt   send_chain;//以ngx_chain_t链表为参数来发送数据的方法

    ngx_listening_t    *listening;//对应的监听对象

    off_t               sent;//这个连接上已发送的数据

    ngx_log_t          *log;

    ngx_pool_t         *pool;//内存池，大小由ngx_listening_s结构中的pool_size决定

    struct sockaddr    *sockaddr;//客户端的sockaddr
    socklen_t           socklen;
    ngx_str_t           addr_text;//客户端的IP字符串形式

#if (NGX_SSL)
    ngx_ssl_connection_t  *ssl;
#endif

    struct sockaddr    *local_sockaddr;//本地监听端口对应local_sockaddr

    ngx_buf_t          *buffer;//用于接收、缓存客户端发来的字节流

    ngx_queue_t         queue;//以双向链表元素的形式加入到ngx_cycle_t的reusable_connections_queue

    ngx_atomic_uint_t   number;//连接次数？

    ngx_uint_t          requests;//请求次数

    unsigned            buffered:8;//缓存中的业务类型

    unsigned            log_error:3;     /* ngx_connection_log_error_e */

    unsigned            single_connection:1;//独立的连接
    unsigned            unexpected_eof:1;
    unsigned            timedout:1;//连接已超时
    unsigned            error:1;//连接处理过程中出现错误
    unsigned            destroyed:1;//连接已销毁

    unsigned            idle:1;//空闲状态
    unsigned            reusable:1;//连接可重用
    unsigned            close:1;//连接已关闭

    unsigned            sendfile:1;//正在发送文件
    unsigned            sndlowat:1;//发送缓冲区必须满足最低设置
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */

#if (NGX_HAVE_IOCP)
    unsigned            accept_context_updated:1;
#endif

#if (NGX_HAVE_AIO_SENDFILE)
    unsigned            aio_sendfile:1;
    ngx_buf_t          *busy_sendfile;
#endif

#if (NGX_THREADS)
    ngx_atomic_t        lock;
#endif
};


ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, void *sockaddr,
    socklen_t socklen);
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_connection(ngx_connection_t *c);
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);
void ngx_free_connection(ngx_connection_t *c);

void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
