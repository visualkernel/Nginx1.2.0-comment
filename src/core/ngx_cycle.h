
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE     16384
#endif


#define NGX_DEBUG_POINTS_STOP   1
#define NGX_DEBUG_POINTS_ABORT  2


typedef struct ngx_shm_zone_s  ngx_shm_zone_t;

typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

struct ngx_shm_zone_s {
    void                     *data;
    ngx_shm_t                 shm;
    ngx_shm_zone_init_pt      init;
    void                     *tag;//标签
};


struct ngx_cycle_s {
    void                  ****conf_ctx;//保存了所有模块的配置项结构体指针，其每个成员指向另外存储着指针的数组
    ngx_pool_t               *pool;//内存池

    ngx_log_t                *log;//读取配置文件之前使用的日志对象
    ngx_log_t                 new_log;//读取配置文件之后使用的日志对象

    ngx_connection_t        **files;
    ngx_connection_t         *free_connections;//可用的连接池
    ngx_uint_t                free_connection_n;

    ngx_queue_t               reusable_connections_queue;//可重复使用的连接队列

    ngx_array_t               listening;//每个成员为ngx_listening_t类型,表示监听端口及相关参数
    ngx_array_t               pathes;
    ngx_list_t                open_files;
    ngx_list_t                shared_memory;//ngx_shm_zone_t

    ngx_uint_t                connection_n;
    ngx_uint_t                files_n;

    ngx_connection_t         *connections;//当前进程上的所有连接对象
    ngx_event_t              *read_events;//所有读事件对象
    ngx_event_t              *write_events;//所有写事件对象

    ngx_cycle_t              *old_cycle;

    ngx_str_t                 conf_file;//配置文件完整路径
    ngx_str_t                 conf_param;//命令行-g指定的配置字符串
    ngx_str_t                 conf_prefix;//配置文件目录路径
    ngx_str_t                 prefix;//安装目录路径
    ngx_str_t                 lock_file;//进程同步的锁文件名
    ngx_str_t                 hostname;//主机名
};

//Core配置项（main上下文）
typedef struct {
     ngx_flag_t               daemon;/* 是否为守护进程，daemon配置项 */
     ngx_flag_t               master;/* 是否为主-工作者进程模式，master_process配置项*/

     ngx_msec_t               timer_resolution;//系统调用gettimeofday的执行频率，timer_resolution配置项

     ngx_int_t                worker_processes; /* 工作者进程数，worker_processes配置项 */
     ngx_int_t                debug_points;//调试点,debug_points配置项

     ngx_int_t                rlimit_nofile;//?
     ngx_int_t                rlimit_sigpending;//限制信号队列的大小，如果信号队列已满，用户再发送的信号会被丢掉，worker_rlimit_sigpending配置项
     off_t                    rlimit_core;//限制coredump产生core文件大小，worker_rlimit_core配置项

     int                      priority;//worker进程的优先级，worker_priority配置项

     ngx_uint_t               cpu_affinity_n;
     uint64_t                *cpu_affinity;//绑定worker进程到指定cpu,worker_cpu_affinity配置项

     char                    *username;//worker进程运行的用户和用户组，user配置项
     ngx_uid_t                user;
     ngx_gid_t                group;

     ngx_str_t                working_directory;//worker进程的工作目录，用于指定core文件的保存目录,working_directory配置项
     ngx_str_t                lock_file;//锁文件，用于实现accept_mutex互斥和共享内存的序列化访问，lock_file配置项

     ngx_str_t                pid;//保存master进程pid的文件路径，pid配置项
     ngx_str_t                oldpid;

     ngx_array_t              env;//设置环境变量，env配置项
     char                   **environment;

#if (NGX_THREADS)
     ngx_int_t                worker_threads;
     size_t                   thread_stack_size;
#endif

} ngx_core_conf_t;


typedef struct {
     ngx_pool_t              *pool;   /* pcre's malloc() pool */
} ngx_core_tls_t;


#define ngx_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);//解析old_cycle提供了配置文件，生成新的cycle
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);
void ngx_delete_pidfile(ngx_cycle_t *cycle);
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);
uint64_t ngx_get_cpu_affinity(ngx_uint_t n);
ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name,
    size_t size, void *tag);


extern volatile ngx_cycle_t  *ngx_cycle;
extern ngx_array_t            ngx_old_cycles;
extern ngx_module_t           ngx_core_module;
extern ngx_uint_t             ngx_test_config;
extern ngx_uint_t             ngx_quiet_mode;
#if (NGX_THREADS)
extern ngx_tls_key_t          ngx_core_tls_key;
#endif


#endif /* _NGX_CYCLE_H_INCLUDED_ */
