/* ENESIM - Drawing Library
 * Copyright (C) 2007-2013 Jorge Luis Zapata
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * This is a naive 'buddy' allocator following Knuth's documentation.
 * The main difference is that we dont store the block information
 * on the block memory itself but on another malloc'd area.
 * This is useful for managing memory which isn't as fast as the main
 * memory like the video memory
 * The algorithm uses an area to store the linked list of blocks.
 * Each block size is equal to the minimum allocatable block size for
 * the memory pool and the number of blocks is equal to the size of the
 * memory pool divided by the block size.
 */
#include "enesim_private.h"

#include "enesim_mempool_buddy_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
/** @cond internal */
#define ENESIM_LOG_DEFAULT _buddy_log

typedef struct _Block
{
	EINA_INLIST;
	Eina_Bool available : 1;
	unsigned short int order : 7; /* final order is order + min_order */
} Block;

typedef struct _Buddy
{
	void *heap; /* start address of the heap */
	size_t size; /* total size in bytes of the heap */
	unsigned int min_order; /* minimum size is 1 << min_order */
	unsigned int max_order; /* maximum size is 1 << max_order */
	unsigned int num_order; /* number of orders */
	Eina_Inlist **areas; /* one area per order */
	Block *blocks; /* the allocated block information */
} Buddy;

static int _init = 0;
static int _buddy_log;

/* get the minimum order greater or equal to size */
static inline unsigned int _get_order(Buddy *b, size_t size)
{
	unsigned int i;
	size_t bytes;

	bytes = 1 << b->min_order;
	for (i = 0; bytes < size && i < b->num_order; i++)
	{
		bytes += bytes;
	}
	DBG("Order for size %zu is %u (%u)", size, i + b->min_order, i);
	return i;
}

static inline void *_get_offset(Buddy *b, Block *block)
{
	void *ret;

	ret = (char *)b->heap + ((block - &b->blocks[0]) << b->min_order);
	return ret;
}

static void *_buddy_init(EINA_UNUSED const char *context,
                   EINA_UNUSED const char *options,
                   va_list args)
{
	Buddy *b;
	int i;
	size_t bytes;
	size_t size;
	size_t min_order;
	void *heap;

	heap = va_arg(args, void *);
	size = va_arg(args, size_t);
	min_order = va_arg(args, int);
	DBG("Creating a buddy allocator with heap %p, size %zu, min_order %zd",
			heap, size, min_order);
	/* the minimum order we support is 15 (32K) */
	min_order = min_order < 15 ? 15 : min_order;
	bytes = 1 << min_order;
	for (i = 0; bytes <= size; i++)
	{
		bytes += bytes;
	}
	if (!i)
	{
		ERR("Impossible to find a 2^k bytes less than size");
		return NULL;
	}

	b = malloc(sizeof(Buddy));
	b->heap = heap;
	b->size = size;
	b->min_order = min_order;
	b->max_order = min_order + i - 1;
	b->num_order = i;
	b->areas = calloc(b->num_order, sizeof(Eina_Inlist *));
	b->blocks = calloc(1 << (b->num_order - 1), sizeof(Block));
	/* setup the initial free area */
	b->blocks[0].available = EINA_TRUE;
	b->areas[b->num_order - 1] = EINA_INLIST_GET(&(b->blocks[0]));

	return b;
}

static void _buddy_shutdown(void *data)
{
	Buddy *b = data;

	free(b->blocks);
	free(b->areas);
	free(b);
}

static void _buddy_free(void *data, void *element)
{
	Buddy *b = data;
	Block *block, *buddy;
	size_t offset;
	size_t idx;

	DBG("Freeing block at %p", element);
	offset = (unsigned char *)element - (unsigned char *)b->heap;
	if (offset > b->size)
		return;

	idx = offset >> b->min_order;
	block = &b->blocks[idx];

	//printf("free %x idx = %d order = %d buddy = %d\n", offset, idx, block->order, idx ^ (1 << block->order));
	/* we should always work with the buddy at right */
	if (idx & (1 << block->order))
	{
		Block *left;

		idx = idx ^ (1 << block->order);
		left = &b->blocks[idx];
		if (!left->available)
			goto end;
		else
		{
			buddy = block;
			block = left;
			b->areas[block->order] = eina_inlist_remove(b->areas[block->order],
					EINA_INLIST_GET(block));
			block->order++;
		}
	}

check:
	/* already on the last order */
	if (block->order + b->min_order == b->max_order)
	{
		goto end; /* get the buddy */
	}

	buddy = &b->blocks[idx ^ (1 << block->order)];
	if (!buddy->available)
	{
		goto end; /* merge two blocks */
	}

	b->areas[block->order] = eina_inlist_remove(b->areas[block->order],
			EINA_INLIST_GET(buddy));
	block->order++;
	goto check;
end:
	/* add the block to the free list */
	block->available = EINA_TRUE;
	b->areas[block->order] = eina_inlist_append(b->areas[block->order],
			EINA_INLIST_GET(block));
}

static void *_buddy_alloc(void *data, unsigned int size)
{
	Buddy *b = data;
	Block *block, *buddy;
	unsigned int k, j;

	DBG("Allocing %u bytes", size);
	k = j = _get_order(b, size);
	/* get a free list of order k where k <= j <= max_order */
	while ((j < b->num_order) && !b->areas[j])
		j++;
	/* check that the order is on our range */
	if (j + b->min_order > b->max_order)
	{
		WRN("Order %d out of range [%d - %d]", j, b->min_order,
				b->max_order);
		return NULL;
	}

	/* get a free element on this order, if not, go splitting until we find one */
	DBG("Getting order %u (%u) for size %u", j, k, size);
found:
	if (j == k)
	{
		void *ret;

		block = EINA_INLIST_CONTAINER_GET(b->areas[j], Block);
		block->available = EINA_FALSE;
		block->order = j;
		/* remove the block from the list */
		b->areas[j] = eina_inlist_remove(b->areas[j], EINA_INLIST_GET(block));
		ret = _get_offset(b, block);

		DBG("Found a block of order %u at %p", j, ret);
		return ret;
	}

	block = EINA_INLIST_CONTAINER_GET(b->areas[j], Block);
	/* split */
	b->areas[j] = eina_inlist_remove(b->areas[j], EINA_INLIST_GET(block));
	j--;
	b->areas[j] = eina_inlist_append(b->areas[j], EINA_INLIST_GET(block));
	buddy = block + (1 << j);
	buddy->order = j;
	buddy->available = EINA_TRUE;
	b->areas[j] = eina_inlist_append(b->areas[j], EINA_INLIST_GET(buddy));

	goto found;
}

static void _buddy_statistics(void *data)
{
	Buddy *b = data;
	unsigned int i;

	printf("Information:\n");
	printf("size = %zu, min_order = %d, max_order = %d, num_order = %d, "
			"num_blocks = %d (%luKB)\n",
			b->size,
			b->min_order,
			b->max_order,
			b->num_order,
			1 << b->num_order,
			(unsigned long)(((1 << (b->num_order)) * sizeof(Block)) / 1024));
	printf("Area dumping:");
	/* iterate over the free lists and dump the maps */
	for (i = 0; i < b->num_order; i++)
	{
		Block *block;

		printf("\n2^%d:", b->min_order + i);
		EINA_INLIST_FOREACH(b->areas[i], block)
		{
			printf(" %zd", (block - &b->blocks[0]));
		}
	}
	printf("\nBlocks dumping:\n");
}

static Eina_Mempool_Backend _backend = {
	/* .name 		= */ "buddy",
	/* .init		= */ _buddy_init,
	/* .free		= */ _buddy_free,
	/* .alloc		= */ _buddy_alloc,
	/* .realloc		= */ NULL,
	/* .garbage_collect	= */ NULL,
	/* .statistics		= */ _buddy_statistics,
	/* .shutdown		= */ _buddy_shutdown,
	/* .repack		= */ NULL,
};

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
int enesim_mempool_buddy_init(void)
{
	if (!_init++)
	{
		eina_mempool_register(&_backend);
		_buddy_log = eina_log_domain_register("buddy", ENESIM_DEFAULT_LOG_COLOR);
	}
	return _init;
}

int enesim_mempool_buddy_shutdown(void)
{
	if (--_init != 0)
		return _init;
	eina_mempool_unregister(&_backend);
	eina_log_domain_unregister(_buddy_log);

	return _init;
}
/** @endcond */