
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_SLAB_PAGE_MASK   3
#define NGX_SLAB_PAGE        0//分配类型，按页,size>=2048
#define NGX_SLAB_BIG         1//大块，size>64
#define NGX_SLAB_EXACT       2//精确，size=64
#define NGX_SLAB_SMALL       3//小块，size<64

#if (NGX_PTR_SIZE == 4)//32bit

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffff
#define NGX_SLAB_PAGE_START  0x80000000

#define NGX_SLAB_SHIFT_MASK  0x0000000f
#define NGX_SLAB_MAP_MASK    0xffff0000
#define NGX_SLAB_MAP_SHIFT   16

#define NGX_SLAB_BUSY        0xffffffff

#else /* (NGX_PTR_SIZE == 8) *///64bit

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffffffffffff
#define NGX_SLAB_PAGE_START  0x8000000000000000

#define NGX_SLAB_SHIFT_MASK  0x000000000000000f
#define NGX_SLAB_MAP_MASK    0xffffffff00000000
#define NGX_SLAB_MAP_SHIFT   32

#define NGX_SLAB_BUSY        0xffffffffffffffff

#endif


#if (NGX_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)     ngx_memset(p, 0xA5, size)

#else

#if (NGX_HAVE_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)                                                \
    if (ngx_debug_malloc)          ngx_memset(p, 0xA5, size)

#else

#define ngx_slab_junk(p, size)

#endif

#endif

static ngx_slab_page_t *ngx_slab_alloc_pages(ngx_slab_pool_t *pool,
    ngx_uint_t pages);
static void ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages);
static void ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level,
    char *text);


static ngx_uint_t  ngx_slab_max_size;//2048
static ngx_uint_t  ngx_slab_exact_size;//64
static ngx_uint_t  ngx_slab_exact_shift;//6

/*
 * 对pool所在的内存空间进行初始化，生成管理内存的结构体数组和分级数组。
 * */
void
ngx_slab_init(ngx_slab_pool_t *pool)
{
    u_char           *p;
    size_t            size;
    ngx_int_t         m;
    ngx_uint_t        i, n, pages;
    ngx_slab_page_t  *slots;

    /* STUB */
    if (ngx_slab_max_size == 0) {
        ngx_slab_max_size = ngx_pagesize / 2;//2048,ngx_pagesize=4096
        ngx_slab_exact_size = ngx_pagesize / (8 * sizeof(uintptr_t));//amd64,4096/64=64
        for (n = ngx_slab_exact_size; n >>= 1; ngx_slab_exact_shift++) {//ngx_slab_exact_shift=6
            /* void */
        }
    }
    /**/

    pool->min_size = 1 << pool->min_shift;//1<<3=8

	//跳过ngx_slab_pool_t已占用的内存空间
    p = (u_char *) pool + sizeof(ngx_slab_pool_t);
    size = pool->end - p;//剩余空间

    ngx_slab_junk(p, size);

	//将剩余空间划分为9个ngx_slab_page_t
    slots = (ngx_slab_page_t *) p;
    n = ngx_pagesize_shift - pool->min_shift;//12-3=9
	//分0~8级（8,16,32,64,128,256,512,1024,2048）
	//每个分级对应的页内存划分为4096/(2^(i+3))个存储单元,最多可分为512个单元(i=0)，最少2个单元(i=8)
	//slots[0]负责[1,8]区间大小的管理,单元大小8,1页可划分512个单元
	//slots[1]负责[9,16]区间大小的管理,单元大小16,1页可划分256个单元
	//slots[2]负责[17,32]区间大小的管理,单元大小32,1页可划分128个单元
	//slots[3]负责[33,64]区间大小的管理,单元大小64,1页可划分64个单元
	//slots[4]负责[65,128]区间大小的管理,单元大小128,1页可划分32个单元
	//slots[5]负责[129,256]区间大小的管理,单元大小256,1页可划分26个单元
	//slots[6]负责[257,512]区间大小的管理,单元大小512,1页可划分8个单元
	//slots[7]负责[513,1024]区间大小的管理,单元大小1024,1页可划分4个单元
	//slots[8]负责[1025,2048]区间大小的管理,单元大小2048,1页可划分2个单元
	//分级分别使用bitmap来标识哪些单元空闲或正使用
    for (i = 0; i < n; i++) {
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }
	//跳过9个分级ngx_slab_page_t占用的内存空间
    p += n * sizeof(ngx_slab_page_t);
	//计算剩余空间（仅去除ngx_slab_pool_t大小）可划分的页面数目
    pages = (ngx_uint_t) (size / (ngx_pagesize + sizeof(ngx_slab_page_t)));

    ngx_memzero(p, pages * sizeof(ngx_slab_page_t));

    pool->pages = (ngx_slab_page_t *) p;

    pool->free.prev = 0;
    pool->free.next = (ngx_slab_page_t *) p;

    pool->pages->slab = pages;
    pool->pages->next = &pool->free;
    pool->pages->prev = (uintptr_t) &pool->free;
	//跳过pages个ngx_slab_page_t占用内存空间
    pool->start = (u_char *)
                  ngx_align_ptr((uintptr_t) p + pages * sizeof(ngx_slab_page_t),
                                 ngx_pagesize);//按4K进行地址齐，即每页的地址的低12位为0
	//计算剩余空间可划分页的数量
    m = pages - (pool->end - pool->start) / ngx_pagesize;
    if (m > 0) {
        pages -= m;
        pool->pages->slab = pages;
    }

    pool->log_ctx = &pool->zero;
    pool->zero = '\0';
}

/* 申请内存(使用互斥锁) */
void *
ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    ngx_shmtx_lock(&pool->mutex);

    p = ngx_slab_alloc_locked(pool, size);

    ngx_shmtx_unlock(&pool->mutex);

    return p;
}

/* 申请内存(未使用互斥锁) */
void *
ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size)
{
    size_t            s;
    uintptr_t         p, n, m, mask, *bitmap;
    ngx_uint_t        i, slot, shift, map;
    ngx_slab_page_t  *page, *prev, *slots;

	//如果申请的空间>=ngx_slab_max_size(2048)，则直接申请页面
	//否则，按分级进行分配
    if (size >= ngx_slab_max_size) {

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                       "slab alloc: %uz", size);

        page = ngx_slab_alloc_pages(pool, (size + ngx_pagesize - 1)
                                          >> ngx_pagesize_shift);
        if (page) {
            p = (page - pool->pages) << ngx_pagesize_shift;
            p += (uintptr_t) pool->start;

        } else {
            p = 0;
        }

        goto done;
    }
	//找到与申请大小匹配的分级slot
    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        size = pool->min_size;
        shift = pool->min_shift;
        slot = 0;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                   "slab alloc: %uz slot: %ui", size, slot);

    slots = (ngx_slab_page_t *) ((u_char *) pool + sizeof(ngx_slab_pool_t));
    page = slots[slot].next;
	//第一次分级分配时，page->next与page相同，
	//判断slots[slot]上有无分配页,page->next != page表示已分配页
    if (page->next != page) {

        if (shift < ngx_slab_exact_shift) {//size < ngx_slab_exact_size

            do {
				//获取页面管理结构page对应的页面内存
				//(page - pool->pages)=>页面编号
				//(page - pool->pages) << ngx_pagesize_shift为对应编号的偏移地址
				//pool->start为第0个页面的地址
				//获取bitmap
                p = (page - pool->pages) << ngx_pagesize_shift;
                bitmap = (uintptr_t *) (pool->start + p);
				//计算bitmap数量
                map = (1 << (ngx_pagesize_shift - shift))
                          / (sizeof(uintptr_t) * 8);
				//根据bitmap数组查找出空闲块
                for (n = 0; n < map; n++) {

                    if (bitmap[n] != NGX_SLAB_BUSY) {//判断是否还存在空闲块

                        for (m = 1, i = 0; m; m <<= 1, i++) {//i表示第几个块
                            if ((bitmap[n] & m)) {//如果块被占用，直接查找下一个
                                continue;
                            }

                            bitmap[n] |= m;//将对应bit置1

							//计算块偏移量
                            i = ((n * sizeof(uintptr_t) * 8) << shift)
                                + (i << shift);

                            if (bitmap[n] == NGX_SLAB_BUSY) {
                                for (n = n + 1; n < map; n++) {
                                     if (bitmap[n] != NGX_SLAB_BUSY) {
                                         p = (uintptr_t) bitmap + i;

                                         goto done;
                                     }
                                }

								//如果页中的块全部被使用，则从slots链中分离
                                prev = (ngx_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                                prev->next = page->next;
                                page->next->prev = page->prev;

                                page->next = NULL;//关键，后面作为判断页已满的条件
                                page->prev = NGX_SLAB_SMALL;
                            }

                            p = (uintptr_t) bitmap + i;

                            goto done;
                        }
                    }
                }

                page = page->next;

            } while (page);

        } else if (shift == ngx_slab_exact_shift) { //size = ngx_slab_exact_size

            do {
                if (page->slab != NGX_SLAB_BUSY) {//此时page->slab相当于bitmap
												  //64字节分级的1页可划分为64个单元
                    for (m = 1, i = 0; m; m <<= 1, i++) {//i表示第几个块
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if (page->slab == NGX_SLAB_BUSY) {
                            prev = (ngx_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NGX_SLAB_EXACT;
                        }

                        p = (page - pool->pages) << ngx_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);

        } else { /* shift > ngx_slab_exact_shift *///size > ngx_slab_exact_size

            n = ngx_pagesize_shift - (page->slab & NGX_SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t) 1 << n) - 1;
            mask = n << NGX_SLAB_MAP_SHIFT;//n * 2^32

            do {
                if ((page->slab & NGX_SLAB_MAP_MASK) != mask) {

                    for (m = (uintptr_t) 1 << NGX_SLAB_MAP_SHIFT, i = 0;
                         m & mask;
                         m <<= 1, i++)//i表示第几个块
                    {
                        if ((page->slab & m)) {//page->slab高32位相当于bitmap
                            continue;          //（>64的分级1页最多只能划分32个单元）
                        }

                        page->slab |= m;//将对应bit置1

                        if ((page->slab & NGX_SLAB_MAP_MASK) == mask) {
                            prev = (ngx_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NGX_SLAB_BIG;
                        }

                        p = (page - pool->pages) << ngx_pagesize_shift;
                        p += i << shift;//跳过i个块
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);
        }
    }
	//分配1个页面空间，得到对应页面的管理结构
    page = ngx_slab_alloc_pages(pool, 1);
	//页面申请成功
    if (page) {
        if (shift < ngx_slab_exact_shift) {//shift < 6，使用bitmap来记录使用情况
			//访问页内存的位图
            p = (page - pool->pages) << ngx_pagesize_shift;
            bitmap = (uintptr_t *) (pool->start + p);

            s = 1 << shift;//单元大小
			//1 << (ngx_pagesize_shift - shift)表示1页分为多少个单元
			//(1 << (ngx_pagesize_shift - shift)) / 8 需要使用多少个字节来表示这些单元
			//1bit表示一个块，1个块有8*s个bit, 计算需要多少个块来表示所有块的数量
            n = (1 << (ngx_pagesize_shift - shift)) / 8 / s;

            if (n == 0) {
                n = 1;
            }
			
			//设置bitmap(bitmap[0]的低n+1(why?)个bit记录所有bitmap信息自身所占用的块的使用情况)
            bitmap[0] = (2 << n) - 1;
			//计算需要多个64bit的bitmap来记录内存块的使用情况
			//每个bit表示一个块
            map = (1 << (ngx_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);//bitmap的个数

            for (i = 1; i < map; i++) {
                bitmap[i] = 0;
            }
			//将page挂接到slots上
            page->slab = shift;//chunk对应的shift
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;

            slots[slot].next = page;

			//s * n表示bitmap已占用的字节数
            p = ((page - pool->pages) << ngx_pagesize_shift) + s * n;
            p += (uintptr_t) pool->start;

            goto done;

        } else if (shift == ngx_slab_exact_shift) {//shift = 6

            page->slab = 1;//设置bitmap
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;

            slots[slot].next = page;

            p = (page - pool->pages) << ngx_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;

        } else { /* shift > ngx_slab_exact_shift , shift > 6*/

            page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;//bitmap+shift
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;

            slots[slot].next = page;

            p = (page - pool->pages) << ngx_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;
        }
    }

    p = 0;

done:

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0, "slab alloc: %p", p);

    return (void *) p;
}


/* 释放内存，与ngx_slab_alloc对应 */
void
ngx_slab_free(ngx_slab_pool_t *pool, void *p)
{
    ngx_shmtx_lock(&pool->mutex);

    ngx_slab_free_locked(pool, p);

    ngx_shmtx_unlock(&pool->mutex);
}

/* 释放内存，与ngx_slab_alloc_locked对应 */
void
ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    ngx_uint_t        n, type, slot, shift, map;
    ngx_slab_page_t  *slots, *page;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0, "slab free: %p", p);
	//检测释放的内存地址是否有效
    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        ngx_slab_error(pool, NGX_LOG_ALERT, "ngx_slab_free(): outside of pool");
        goto fail;
    }
	//找到p所在的内存页和页管理结构体
    n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;//页编号
    page = &pool->pages[n];//对应的页管理结构体
    slab = page->slab;
    type = page->prev & NGX_SLAB_PAGE_MASK;//获取低2bit

    switch (type) {

    case NGX_SLAB_SMALL://小块
		//获取块的大小
        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {//?
            goto wrong_chunk;
        }
		//p & (ngx_pagesize - 1)相当于p块在对应页的偏移量(字节)
        n = ((uintptr_t) p & (ngx_pagesize - 1)) >> shift;//计算p块属于第几个块
        m = (uintptr_t) 1 << (n & (sizeof(uintptr_t) * 8 - 1));//计算p所对的bit的掩码，如000100000000000000000
															   //(等价1<<(n%(sizeof(uintptr_t) * 8 - 1)))
        n /= (sizeof(uintptr_t) * 8);//计算管理p块的bit所在的bitmap的下标
        bitmap = (uintptr_t *) ((uintptr_t) p & ~(ngx_pagesize - 1));//计算p块所在的内存页地址

        if (bitmap[n] & m) {
			//如果page已满，释放某块后需要重新挂载到slots[slot]上以供下次使用
            if (page->next == NULL) {
                slots = (ngx_slab_page_t *)
                                   ((u_char *) pool + sizeof(ngx_slab_pool_t));
                slot = shift - pool->min_shift;
				//将页重新挂到slots[slot]链中
                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | NGX_SLAB_SMALL;
            }
			//清理bitmap
            bitmap[n] &= ~m;

            n = (1 << (ngx_pagesize_shift - shift)) / 8 / (1 << shift);

            if (n == 0) {
                n = 1;
            }
			//bitmap[0]低n位用于表示bitmap数组本身的占用块情况
			//其它bit用于表示用户数用占用块的情况
            if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                goto done;
            }

            map = (1 << (ngx_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);
			
			//如果页上的所有块都是空闲，则释放该页
            for (n = 1; n < map; n++) {
                if (bitmap[n]) {
                    goto done;
                }
            }
			
            ngx_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_EXACT://精确块
		//计算p所对的bit的掩码，如000100000000000000000
        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (ngx_pagesize - 1)) >> ngx_slab_exact_shift);
        size = ngx_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        if (slab & m) {
            if (slab == NGX_SLAB_BUSY) {//如果当前页所有块都被使用
                slots = (ngx_slab_page_t *)
                                   ((u_char *) pool + sizeof(ngx_slab_pool_t));
                slot = ngx_slab_exact_shift - pool->min_shift;
				//将该页重新挂入slots[slot]链中
                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | NGX_SLAB_EXACT;
            }

            page->slab &= ~m;

            if (page->slab) {
                goto done;
            }

            ngx_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_BIG://大块
		//计算块大小
        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }
		//计算p所对的bit的掩码，如000100000000000000000
        m = (uintptr_t) 1 << ((((uintptr_t) p & (ngx_pagesize - 1)) >> shift)
                              + NGX_SLAB_MAP_SHIFT);

        if (slab & m) {

            if (page->next == NULL) {
                slots = (ngx_slab_page_t *)
                                   ((u_char *) pool + sizeof(ngx_slab_pool_t));
                slot = shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;
                page->next->prev = (uintptr_t) page | NGX_SLAB_BIG;
            }

            page->slab &= ~m;

            if (page->slab & NGX_SLAB_MAP_MASK) {
                goto done;
            }

            ngx_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_PAGE://按页

        if ((uintptr_t) p & (ngx_pagesize - 1)) {
            goto wrong_chunk;
        }

        if (slab == NGX_SLAB_PAGE_FREE) {
            ngx_slab_error(pool, NGX_LOG_ALERT,
                           "ngx_slab_free(): page is already free");
            goto fail;
        }

        if (slab == NGX_SLAB_PAGE_BUSY) {
            ngx_slab_error(pool, NGX_LOG_ALERT,
                           "ngx_slab_free(): pointer to wrong page");
            goto fail;
        }
		//计算第几页(从0开始)
        n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;
        size = slab & ~NGX_SLAB_PAGE_START;//页数

        ngx_slab_free_pages(pool, &pool->pages[n], size);

        ngx_slab_junk(p, size << ngx_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    ngx_slab_junk(p, size);

    return;

wrong_chunk:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): pointer to wrong chunk");

    goto fail;

chunk_already_free:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): chunk is already free");

fail:

    return;
}

/* 申请pages个连续页, 返回首个页管理结构 */
static ngx_slab_page_t *
ngx_slab_alloc_pages(ngx_slab_pool_t *pool, ngx_uint_t pages)
{
    ngx_slab_page_t  *page, *p;
	/* 遍历所有空闲页面 */
    for (page = pool->free.next; page != &pool->free; page = page->next) {
	
        if (page->slab >= pages) {

            if (page->slab > pages) {
                page[pages].slab = page->slab - pages;//剩余连续空闲页的数目
                page[pages].next = page->next;
                page[pages].prev = page->prev;
				
                p = (ngx_slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            } else {//page-slab == pages
				//将pages个页面从链表中分离
                p = (ngx_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | NGX_SLAB_PAGE_START;//申请页面数
            page->next = NULL;
            page->prev = NGX_SLAB_PAGE;//按页分配

            if (--pages == 0) {
                return page;
            }
			//设置剩余连续页管理结构状态
            for (p = page + 1; pages; pages--) {
                p->slab = NGX_SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = NGX_SLAB_PAGE;//按页分配
                p++;
            }

            return page;
        }
    }

    ngx_slab_error(pool, NGX_LOG_CRIT, "ngx_slab_alloc() failed: no memory");

    return NULL;
}

/* 释放pages个连续页 */
static void
ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages)
{
    ngx_slab_page_t  *prev;

    page->slab = pages--;//why?

    if (pages) {//将剩余连续的管理结构体清0
        ngx_memzero(&page[1], pages * sizeof(ngx_slab_page_t));
    }

    if (page->next) {
        prev = (ngx_slab_page_t *) (page->prev & ~NGX_SLAB_PAGE_MASK);//将prev低2bit清0
        prev->next = page->next;
        page->next->prev = page->prev;
    }

	//将page加入free链的头部
    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


static void
ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level, char *text)
{
    ngx_log_error(level, ngx_cycle->log, 0, "%s%s", text, pool->log_ctx);
}
