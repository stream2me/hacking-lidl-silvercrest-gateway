// SPDX-License-Identifier: GPL-2.0
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/stddef.h>
#include "rtl8196e_pool.h"

#define RTL8196E_POOL_MAGIC "819X"
#define RTL8196E_POOL_MAGIC_LEN 4
#define RTL8196E_PRIV_DATA_SIZE 128

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

/* External cache for sk_buff struct allocation */
extern struct kmem_cache *skbuff_head_cache;

static struct sk_buff *rtl8196e_build_skb(unsigned char *data, int size)
{
	struct sk_buff *skb;
	struct skb_shared_info *shinfo;

	if (!data)
		return NULL;

	skb = kmem_cache_alloc(skbuff_head_cache, GFP_ATOMIC & ~__GFP_DMA);
	if (!skb)
		return NULL;

	memset(skb, 0, offsetof(struct sk_buff, truesize));
	refcount_set(&skb->users, 1);
	skb->head = data;
	skb->data = data;
	skb->tail = data;

	size = SKB_DATA_ALIGN(size + RTL8196E_PRIV_DATA_SIZE + NET_SKB_PAD);
	skb->end = data + size;
	skb->truesize = size + sizeof(struct sk_buff);

	shinfo = skb_shinfo(skb);
	atomic_set(&shinfo->dataref, 1);
	shinfo->nr_frags = 0;
	shinfo->gso_size = 0;
	shinfo->gso_segs = 0;
	shinfo->gso_type = 0;
	shinfo->frag_list = NULL;

	skb->head_frag = 0;
	skb_reserve(skb, RTL8196E_PRIV_DATA_SIZE);

	return skb;
}

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
	pool->buf_size = buf_size;
	pool->count = count;
	pool->free_count = 0;

	for (i = 0; i < count; i++) {
		struct rtl8196e_pool_buf *b;
		b = kmalloc(sizeof(*b) + buf_size, GFP_KERNEL | GFP_DMA);
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
