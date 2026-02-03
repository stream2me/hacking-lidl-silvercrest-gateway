// SPDX-License-Identifier: GPL-2.0
/*
 * RTL8196E private RX buffer pool.
 *
 * Maintains a stable-address buffer pool and builds SKBs on top of it to
 * match hardware expectations and avoid realloc/free churn.
 */
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/stddef.h>
#include "rtl8196e_pool.h"

#define RTL8196E_POOL_MAGIC "819X"
#define RTL8196E_POOL_MAGIC_LEN 4
#define RTL8196E_PRIV_DATA_SIZE 128

/**
 * rtl8196e_skb_alloc_size() - Compute total allocation size for an SKB.
 * @payload: Payload length in bytes.
 *
 * Return: Total allocation size for SKB data + shared info.
 */
static inline size_t rtl8196e_skb_alloc_size(size_t payload)
{
	size_t data_len = SKB_DATA_ALIGN(payload + RTL8196E_PRIV_DATA_SIZE + NET_SKB_PAD);
	size_t shinfo_len = SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

	return data_len + shinfo_len;
}

struct rtl8196e_pool_buf {
	char magic[RTL8196E_POOL_MAGIC_LEN];
	void *buf_pointer;
	struct rtl8196e_pool *pool;
	unsigned char data[];
};

struct rtl8196e_pool {
	unsigned int buf_size;
	int count;
	spinlock_t lock;
	struct rtl8196e_pool_buf **free_list;
	int free_count;
};

/* External cache for sk_buff struct allocation. */
extern struct kmem_cache *skbuff_head_cache;

/**
 * rtl8196e_build_skb() - Build an SKB around a preallocated data buffer.
 * @data: Buffer start.
 * @size: Buffer size.
 *
 * Return: New SKB or NULL on failure.
 */
static struct sk_buff *rtl8196e_build_skb(unsigned char *data, int size)
{
	struct sk_buff *skb;
	struct skb_shared_info *shinfo;
	size_t alloc_size;
	size_t data_size;

	if (!data)
		return NULL;

	/* SKB header from slab, data from the private pool. */
	skb = kmem_cache_alloc(skbuff_head_cache, GFP_ATOMIC & ~__GFP_DMA);
	if (!skb)
		return NULL;

	memset(skb, 0, offsetof(struct sk_buff, tail));
	refcount_set(&skb->users, 1);
	skb->head = data;
	skb->data = data;
	skb_reset_tail_pointer(skb);

	alloc_size = rtl8196e_skb_alloc_size(size);
	data_size = alloc_size - SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
	skb->end = skb->tail + data_size;
	skb->truesize = SKB_TRUESIZE(data_size);
	skb->mac_header = (typeof(skb->mac_header))~0U;
	skb->transport_header = (typeof(skb->transport_header))~0U;

	shinfo = skb_shinfo(skb);
	memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
	atomic_set(&shinfo->dataref, 1);

	/* Leave headroom for driver-private data and NET_SKB_PAD. */
	skb_reserve(skb, RTL8196E_PRIV_DATA_SIZE + NET_SKB_PAD);

	return skb;
}

/**
 * rtl8196e_pool_create() - Create a private buffer pool.
 * @buf_size: Desired payload size.
 * @count: Number of buffers to allocate.
 *
 * Return: Pool handle or NULL on failure.
 */
struct rtl8196e_pool *rtl8196e_pool_create(size_t buf_size, int count)
{
	struct rtl8196e_pool *pool;
	int i;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	pool->free_list = kcalloc(count, sizeof(*pool->free_list), GFP_KERNEL);
	if (!pool->free_list) {
		kfree(pool);
		return NULL;
	}

	spin_lock_init(&pool->lock);
	pool->buf_size = rtl8196e_skb_alloc_size(buf_size);
	pool->count = count;
	pool->free_count = 0;

	for (i = 0; i < count; i++) {
		struct rtl8196e_pool_buf *b;
		/* Allocate stable DMA-safe buffer with metadata prefix. */
		b = kmalloc(sizeof(*b) + pool->buf_size, GFP_KERNEL | GFP_DMA);
		if (!b)
			break;
		memcpy(b->magic, RTL8196E_POOL_MAGIC, RTL8196E_POOL_MAGIC_LEN);
		b->buf_pointer = b;
		b->pool = pool;
		pool->free_list[pool->free_count++] = b;
	}

	if (pool->free_count == 0) {
		rtl8196e_pool_destroy(pool);
		return NULL;
	}

	return pool;
}

/**
 * rtl8196e_pool_destroy() - Destroy a pool and free its buffers.
 * @pool: Pool handle.
 */
void rtl8196e_pool_destroy(struct rtl8196e_pool *pool)
{
	int i;

	if (!pool)
		return;

	for (i = 0; i < pool->free_count; i++)
		kfree(pool->free_list[i]);

	kfree(pool->free_list);
	kfree(pool);
}

/**
 * rtl8196e_pool_alloc() - Allocate a raw buffer from the pool.
 * @pool: Pool handle.
 * @skb_out: Optional pointer, always set to NULL (unused).
 *
 * Return: Buffer pointer or NULL if pool empty.
 */
void *rtl8196e_pool_alloc(struct rtl8196e_pool *pool, void **skb_out)
{
	unsigned long flags;
	struct rtl8196e_pool_buf *b;

	if (!pool)
		return NULL;

	spin_lock_irqsave(&pool->lock, flags);
	if (pool->free_count == 0) {
		spin_unlock_irqrestore(&pool->lock, flags);
		return NULL;
	}

	b = pool->free_list[--pool->free_count];
	spin_unlock_irqrestore(&pool->lock, flags);

	if (skb_out)
		*skb_out = NULL;

	return b->data;
}

/**
 * rtl8196e_pool_free() - Return a raw buffer to the pool.
 * @pool: Pool handle.
 * @buf: Buffer pointer.
 */
void rtl8196e_pool_free(struct rtl8196e_pool *pool, void *buf)
{
	unsigned long flags;
	struct rtl8196e_pool_buf *b;

	if (!pool || !buf)
		return;

	b = (struct rtl8196e_pool_buf *)((unsigned long)buf - offsetof(struct rtl8196e_pool_buf, data));

	spin_lock_irqsave(&pool->lock, flags);
	pool->free_list[pool->free_count++] = b;
	spin_unlock_irqrestore(&pool->lock, flags);
}

/**
 * rtl8196e_pool_alloc_skb() - Allocate an SKB backed by pool memory.
 * @pool: Pool handle.
 * @size: Payload size.
 *
 * Return: SKB pointer or NULL on failure.
 */
struct sk_buff *rtl8196e_pool_alloc_skb(struct rtl8196e_pool *pool, unsigned int size)
{
	unsigned char *buf;
	struct sk_buff *skb;

	buf = rtl8196e_pool_alloc(pool, NULL);
	if (!buf)
		return NULL;

	skb = rtl8196e_build_skb(buf, size);
	if (!skb) {
		rtl8196e_pool_free(pool, buf);
		return NULL;
	}

	return skb;
}

/**
 * is_rtl865x_eth_priv_buf() - Identify pool-backed SKB data.
 * @head: SKB head pointer.
 *
 * Return: 1 if buffer belongs to the RTL8196E pool, 0 otherwise.
 */
int is_rtl865x_eth_priv_buf(unsigned char *head)
{
	struct rtl8196e_pool_buf *b;

	if (!head)
		return 0;

	b = (struct rtl8196e_pool_buf *)((unsigned long)head - offsetof(struct rtl8196e_pool_buf, data));

	if (memcmp(b->magic, RTL8196E_POOL_MAGIC, RTL8196E_POOL_MAGIC_LEN) != 0)
		return 0;

	return b->buf_pointer == (void *)b;
}
EXPORT_SYMBOL(is_rtl865x_eth_priv_buf);

/**
 * free_rtl865x_eth_priv_buf() - Return pool-backed SKB data to pool.
 * @head: SKB head pointer.
 */
void free_rtl865x_eth_priv_buf(unsigned char *head)
{
	struct rtl8196e_pool_buf *b;
	if (!head)
		return;

	b = (struct rtl8196e_pool_buf *)((unsigned long)head - offsetof(struct rtl8196e_pool_buf, data));
	if (!b->pool)
		return;

	rtl8196e_pool_free(b->pool, head);
}
EXPORT_SYMBOL(free_rtl865x_eth_priv_buf);
