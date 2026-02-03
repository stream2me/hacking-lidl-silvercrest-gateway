/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RTL8196E private buffer pool interface.
 */
#ifndef RTL8196E_POOL_H
#define RTL8196E_POOL_H

#include <linux/types.h>

struct sk_buff;
struct rtl8196e_pool;

/**
 * rtl8196e_pool_create() - Create a private buffer pool.
 * @buf_size: Payload size per buffer.
 * @count: Number of buffers to allocate.
 *
 * Return: Pool handle or NULL on failure.
 */
struct rtl8196e_pool *rtl8196e_pool_create(size_t buf_size, int count);

/**
 * rtl8196e_pool_destroy() - Destroy a private buffer pool.
 * @pool: Pool handle.
 */
void rtl8196e_pool_destroy(struct rtl8196e_pool *pool);

/**
 * rtl8196e_pool_alloc() - Allocate a raw buffer from the pool.
 * @pool: Pool handle.
 * @skb_out: Optional SKB pointer output (unused).
 *
 * Return: Buffer pointer or NULL on failure.
 */
void *rtl8196e_pool_alloc(struct rtl8196e_pool *pool, void **skb_out);

/**
 * rtl8196e_pool_free() - Return a raw buffer to the pool.
 * @pool: Pool handle.
 * @buf: Buffer pointer.
 */
void rtl8196e_pool_free(struct rtl8196e_pool *pool, void *buf);

/**
 * rtl8196e_pool_alloc_skb() - Allocate an SKB backed by pool memory.
 * @pool: Pool handle.
 * @size: Payload size.
 *
 * Return: SKB pointer or NULL on failure.
 */
struct sk_buff *rtl8196e_pool_alloc_skb(struct rtl8196e_pool *pool, unsigned int size);

/* Kernel patch hooks (keep symbol names for existing patch). */
/**
 * is_rtl865x_eth_priv_buf() - Identify pool-backed SKB data.
 * @head: SKB head pointer.
 *
 * Return: 1 if buffer belongs to the pool, 0 otherwise.
 */
int is_rtl865x_eth_priv_buf(unsigned char *head);

/**
 * free_rtl865x_eth_priv_buf() - Return pool-backed SKB data to pool.
 * @head: SKB head pointer.
 */
void free_rtl865x_eth_priv_buf(unsigned char *head);

#endif /* RTL8196E_POOL_H */
