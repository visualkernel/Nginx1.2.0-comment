
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SLAB_H_INCLUDED_
#define _NGX_SLAB_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_slab_page_s  ngx_slab_page_t;

struct ngx_slab_page_s {
	/**
	slab可能存储的值为：
	    a.存储为些结构相连的pages的数目(slab page管理结构)
	　　b.存储标记chunk使用情况的bitmap(size = exact_size)
	　　c.存储chunk的大小(size < exact_size)
	　　d.存储标记chunk的使用情况及chunk大小(size > exact_size)
     */
    uintptr_t         slab;
    ngx_slab_page_t  *next;
    uintptr_t         prev;
};

/* slab内存池:对共享内存进一步的内部划分与管理 */
typedef struct {
    ngx_shmtx_sh_t    lock;

    size_t            min_size;/*最小分配单元字节大小,8字节*/
    size_t            min_shift;/*最小分配单元字节大小对应的位移,3（2^3=8）*/

    ngx_slab_page_t  *pages;/* 页数组 */
    ngx_slab_page_t   free; /* 空闲页链表 */

    u_char           *start; /* 可分配空间的起始位置 */
    u_char           *end;/* 内存空间的结束地址 */

    ngx_shmtx_t       mutex;

    u_char           *log_ctx;
    u_char            zero;

    void             *data;
    void             *addr;
} ngx_slab_pool_t;


void ngx_slab_init(ngx_slab_pool_t *pool);
void *ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size);
void ngx_slab_free(ngx_slab_pool_t *pool, void *p);
void ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p);


#endif /* _NGX_SLAB_H_INCLUDED_ */
