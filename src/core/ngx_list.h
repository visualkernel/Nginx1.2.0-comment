
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LIST_H_INCLUDED_
#define _NGX_LIST_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_list_part_s  ngx_list_part_t;

/* 组成列表的单个部件 */
struct ngx_list_part_s {
    void             *elts;/* 元素存储内存首地址 */
    ngx_uint_t        nelts;/*已保存的元素个数 */
    ngx_list_part_t  *next;/* 列表的下一部件 */
};


typedef struct {
    ngx_list_part_t  *last;/*最后一个部件*/
    ngx_list_part_t   part;/*第一个部件*/
    size_t            size;/*单个元素的字节大小*/
    ngx_uint_t        nalloc;/* 单个部件能存储的元素最大个数 */
    ngx_pool_t       *pool;/* 用于分配内存空间的内存池 */
} ngx_list_t;

/* 创建列表 */
ngx_list_t *ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);

/* 对一个未分配数据存储空间的列表进行初始化 */
static ngx_inline ngx_int_t
ngx_list_init(ngx_list_t *list, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    list->part.elts = ngx_palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}


/*
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */

/* 获取保存新元素的内存地址 */
void *ngx_list_push(ngx_list_t *list);


#endif /* _NGX_LIST_H_INCLUDED_ */
