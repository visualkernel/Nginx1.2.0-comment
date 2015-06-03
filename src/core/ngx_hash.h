
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HASH_H_INCLUDED_
#define _NGX_HASH_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    void             *value;/*值,空槽时为NULL*/
    u_short           len; /* name长度 */
    u_char            name[1];/*键*/
} ngx_hash_elt_t;


typedef struct {
    ngx_hash_elt_t  **buckets;/* 桶*/
    ngx_uint_t        size;/*桶的个数*/
} ngx_hash_t;


typedef struct {
    ngx_hash_t        hash;
    void             *value;/*当作为某容器的元素时，可以使用这个value指向用户数据*/
} ngx_hash_wildcard_t;


typedef struct {
    ngx_str_t         key;/*键*/
    ngx_uint_t        key_hash;/*由hash函数计算得到的hash值*/
    void             *value;/*用户数据*/
} ngx_hash_key_t;


typedef ngx_uint_t (*ngx_hash_key_pt) (u_char *data, size_t len);


typedef struct {
    ngx_hash_t            hash;/*精确匹配散列表*/
    ngx_hash_wildcard_t  *wc_head;/*前置通配符散列表*/
    ngx_hash_wildcard_t  *wc_tail;/*后置通配符散列表*/
} ngx_hash_combined_t;


typedef struct {
    ngx_hash_t       *hash;/*指向普通的精确匹配散列表*/
    ngx_hash_key_pt   key;/* hash函数 */

    ngx_uint_t        max_size;/*桶的最大个数*/
    ngx_uint_t        bucket_size;/*每个桶的大小*/

    char             *name;/*散列表名称*/
    ngx_pool_t       *pool;
    ngx_pool_t       *temp_pool;/*主要用于临时分配动态数组*/
} ngx_hash_init_t;


#define NGX_HASH_SMALL            1
#define NGX_HASH_LARGE            2

#define NGX_HASH_LARGE_ASIZE      16384
#define NGX_HASH_LARGE_HSIZE      10007

#define NGX_HASH_WILDCARD_KEY     1
#define NGX_HASH_READONLY_KEY     2


typedef struct {
    ngx_uint_t        hsize;/*hash表的槽个数*/

    ngx_pool_t       *pool;
    ngx_pool_t       *temp_pool;

    ngx_array_t       keys;/*精确键值对数组*/
    ngx_array_t      *keys_hash;/*键名hash表*/

    ngx_array_t       dns_wc_head;/*前置通配符键值对数组*/
    ngx_array_t      *dns_wc_head_hash;/*前置通配符键名hash表*/

    ngx_array_t       dns_wc_tail;/*后置通配符键值对数组*/
    ngx_array_t      *dns_wc_tail_hash;/*后置通配符键名hash表*/
} ngx_hash_keys_arrays_t;


typedef struct {
    ngx_uint_t        hash;
    ngx_str_t         key;
    ngx_str_t         value;
    u_char           *lowcase_key;
} ngx_table_elt_t;

/*查找函数*/
void *ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len);
void *ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);
void *ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);
void *ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key,
    u_char *name, size_t len);

/*初始化函数*/
ngx_int_t ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);
ngx_int_t ngx_hash_wildcard_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);

/*hash函数*/
#define ngx_hash(key, c)   ((ngx_uint_t) key * 31 + c)
ngx_uint_t ngx_hash_key(u_char *data, size_t len);
ngx_uint_t ngx_hash_key_lc(u_char *data, size_t len);
ngx_uint_t ngx_hash_strlow(u_char *dst, u_char *src, size_t n);

/*键数组函数*/
ngx_int_t ngx_hash_keys_array_init(ngx_hash_keys_arrays_t *ha, ngx_uint_t type);
ngx_int_t ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key,
    void *value, ngx_uint_t flags);


#endif /* _NGX_HASH_H_INCLUDED_ */
