#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "omfs.h"
#include "dirscan.h"
#include "check.h"
#include "crc.h"
#include "fix.h"
#include "bits.h"
#include "io.h"

/*
 * TODO: 
 *
 * - check hash table index
 * - clear extents from free bitmap and make sure free bitmap is null
 * - sanity check block size, sys block size, mirrors, etc
 * - check that extent count is valid
 * - check that terminator matches
 * - make sure file sizes match up
 */

int check_crc(u8 *blk)
{
	u16 crc;
	size_t hdr_size = sizeof(struct omfs_header);
	struct omfs_inode *oi;

	oi = (struct omfs_inode *) blk;

        crc = crc_ccitt_msb(0, blk + hdr_size, 
		swap_be32(oi->i_head.h_body_size));

	return (crc == swap_be16(oi->i_head.h_crc));
}

int check_header(u8 *blk)
{
	int i;
	int xor;
	struct omfs_inode *oi;

	oi = (struct omfs_inode *) blk;

	xor = blk[0];
	for (i = 1; i < OMFS_XOR_COUNT; i++)
		xor ^= blk[i];

	return (xor == oi->i_head.h_check_xor);
}

int check_sanity(check_context_t *ctx)
{
	omfs_inode_t *inode = ctx->current_inode;
	if (swap_be32(inode->i_head.h_body_size) > 
	    swap_be32(ctx->omfs_info->super->s_sys_blocksize))
		return 0;

	// check device size here too.
	return 1;
}

int check_bitmap(check_context_t *ctx)
{
	int i;
	int is_ok = 1;
	int bsize, first_blk;
	omfs_super_t *super = ctx->omfs_info->super;
	omfs_root_t *root = ctx->omfs_info->root;

	if (!ctx->bitmap)
		return 0;

	bsize = (swap_be64(super->s_num_blocks) + 7) / 8;
	first_blk = swap_be64(root->r_bitmap) + (bsize + 
		swap_be32(super->s_blocksize)-1) / 
		swap_be32(super->s_blocksize);

	for (i=0; i < first_blk; i++)
		set_bit(ctx->visited, i);

	for (i=0; i < bsize; i++)
	{
		if (ctx->bitmap[i] != ctx->visited[i])
		{
			is_ok = 0;
			if (!ctx->config->is_quiet)
			{
				printf("Wrong bitmap byte at %d (%02x,%02x)\n", 
					i, ctx->bitmap[i], ctx->visited[i]);
			}
		}
	}
	if (!is_ok) 
	{
		fix_problem(E_BITMAP, ctx);
	}
	return is_ok;
}

void visit_extents(check_context_t *ctx)
{
	struct omfs_extent *oe;
	struct omfs_extent_entry *entry;
	u64 last, next;
	int extent_count, i;
	u8 *buf;

	next = ctx->block;
	buf = omfs_get_block(ctx->omfs_info, next);
	if (!buf)
		return;

	oe = (struct omfs_extent *) &buf[OMFS_EXTENT_START];

	for(;;) 
	{
		extent_count = swap_be32(oe->e_extent_count);
		last = next;
		next = swap_be64(oe->e_next);
		entry = &oe->e_entry;

		// ignore last entry as it is the terminator
		for (; extent_count > 1; extent_count--)
		{
			u64 start = swap_be64(entry->e_cluster);

			for (i=0; i<swap_be64(entry->e_blocks); i++)
				set_bit(ctx->visited, start + i);

			entry++;
		}

		set_bit(ctx->visited, last);
		set_bit(ctx->visited, last+1);

		if (next == ~0)
			break;

		free(buf);
		buf = omfs_get_block(ctx->omfs_info, next);
		if (!buf)
			goto err;
		oe = (struct omfs_extent *) &buf[OMFS_EXTENT_CONT];
	}
	free(buf);
err:
	return;
}
	
int check_inode(check_context_t *ctx)
{
	int i;
	int ret = 1;
	omfs_inode_t *inode = ctx->current_inode;

	if (test_bit(ctx->visited, ctx->block))
	{
		fix_problem(E_LOOP, ctx);
		return 0;
	}
	for (i=0; i < swap_be32(ctx->omfs_info->super->s_mirrors); i++)
		set_bit(ctx->visited, ctx->block + i);

	if (!check_sanity(ctx))
	{
		fix_problem(E_INSANE, ctx);
		return 0;
	}
	if (!check_header((u8 *)inode)) 
	{
		fix_problem(E_HEADER_XOR, ctx);
		ret = 0;
	}
	if (!check_crc((u8 *)inode)) 
	{
		fix_problem(E_HEADER_CRC, ctx);
		ret = 0;
	}
	if (swap_be64(inode->i_head.h_self) != ctx->block)
	{
		fix_problem(E_SELF_PTR, ctx);
		ret = 0;
	}
	if (swap_be64(inode->i_parent) != ctx->parent)
	{
		fix_problem(E_PARENT_PTR, ctx);
		ret = 0;
	}
	if (omfs_compute_hash(ctx->omfs_info, inode->i_name) != ctx->hash)
	{
		fix_problem(E_HASH_WRONG, ctx);
		ret = 0;
	}
	if (inode->i_type == OMFS_FILE)
	{
		visit_extents(ctx);
	}

	return ret;
}

static int on_node(dirscan_t *d, dirscan_entry_t *entry, void *user)
{
	check_context_t *ctx = (check_context_t *) user;

	ctx->current_inode = entry->inode;
	ctx->block = entry->block;
	ctx->parent = entry->parent;
	ctx->hash = entry->hindex;
	return check_inode(ctx);
}

int check_fs(FILE *fp, check_fs_config_t *config)
{
	int res;
	check_context_t ctx;
	int bsize;
	omfs_super_t super;
	omfs_root_t root;
	omfs_info_t info = { 
		.dev = fp, 
		.super = &super,
		.root = &root
	};

	ctx.config = config;

	if (omfs_read_super(&info))
	{
		fix_problem(E_READ_SUPER, &ctx);
		return 0;
	}
	if (omfs_read_root_block(&info))
	{
		fix_problem(E_READ_ROOT, &ctx);
		return 0;
	}

	ctx.omfs_info = &info;
	omfs_load_bitmap(&info);
	ctx.bitmap = info.bitmap->bmap;
	bsize = (swap_be64(info.super->s_num_blocks) + 7) / 8;
	ctx.visited = calloc(1, bsize);

	/* FIXME error codes are all over the place. */
	res = dirscan_begin(&info, on_node, &ctx);

	if (res < 0)
	{
		fix_problem(E_SCAN, &ctx);
		return 0;
	}
	if (res != 0)
		return 0;

	res = check_bitmap(&ctx);
	
	if (ctx.bitmap) 
		free(ctx.bitmap);
	free(ctx.visited);
	return res;
}
