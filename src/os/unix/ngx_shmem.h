
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SHMEM_H_INCLUDED_
#define _NGX_SHMEM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    u_char      *addr;/* 共享内存空间的首地址 */
    size_t       size;/* 内存字节大小 */
    ngx_str_t    name;/* 共享内存名称 */
    ngx_log_t   *log;/* 日志对象 */
    ngx_uint_t   exists;   /* unsigned  exists:1;  共享内存是否已分配标志，1-已分配，0-未分配 */
} ngx_shm_t;


ngx_int_t ngx_shm_alloc(ngx_shm_t *shm);
void ngx_shm_free(ngx_shm_t *shm);


#endif /* _NGX_SHMEM_H_INCLUDED_ */
