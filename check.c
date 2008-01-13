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
		swap_be32(oi->head.body_size));

	return (crc == swap_be16(oi->head.crc));
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

	return (xor == oi->head.check_xor);
}

int check_sanity(check_context_t *ctx)
{
	omfs_inode_t *inode = ctx->current_inode;
	if (inode->head.body_size > ctx->omfs_info->super->sys_blocksize)
		return 0;
	// check device size here too.
	return 1;
}

int check_inode(check_context_t *ctx)
{
	int ret = 1;
	omfs_inode_t *inode = ctx->current_inode;
	if (!check_sanity(ctx))
	{
		fix_problem(E_INSANE, ctx);
		return 0;
	}
	if (!check_header(inode->data)) 
	{
		fix_problem(E_HEADER_XOR, ctx);
	}
	if (!check_crc(inode->data)) 
	{
		fix_problem(E_HEADER_CRC, ctx);
	}
	if (inode->head.self != ctx->block)
	{
		fix_problem(E_SELF_PTR, ctx);
		ret = 0;
	}
	if (inode->parent != ctx->parent)
	{
		fix_problem(E_PARENT_PTR, ctx);
		ret = 0;
	}
	if (omfs_compute_hash(ctx->omfs_info, inode->name) != ctx->hash)
	{
		fix_problem(E_HASH_WRONG, ctx);
		ret = 0;
	}
	if (ctx->bitmap && !test_bit(ctx->bitmap, inode->head.self))
	{
		fix_problem(E_BIT_CLEAR, ctx);
		ret = 0;
	}
	clear_bit(ctx->bitmap, inode->head.self);
	if (inode->type == OMFS_FILE)
	{
		// TODO check its extents here
	}

	return ret;
}

int check_fs(FILE *fp)
{
	check_context_t ctx;
	omfs_super_t super;
	omfs_root_t root;
	omfs_info_t info = { 
		.dev = fp, 
		.super = &super,
		.root = &root
	};

	if (omfs_read_super(fp, &super))
	{
		fix_problem(E_READ_SUPER, 0);
		return 0;
	}
	if (omfs_read_root_block(fp, &super, &root))
	{
		fix_problem(E_READ_ROOT, 0);
		return 0;
	}

	dirscan_entry_t *entry;
	ctx.omfs_info = &info;
	ctx.bitmap = omfs_get_bitmap(&info);

	dirscan_t *scan = dirscan_begin(&info);
	if (!scan)
	{
		fix_problem(E_SCAN, 0);
		return 0;
	}

	while ((entry = dirscan_next(scan)))
	{
		char *name = escape(entry->inode->name);
		printf("inode: %*c%s %llx %d %llx %llx\n", 
			entry->level*2, ' ', name,
			entry->inode->head.self, entry->hindex,
			entry->parent, entry->block); 
		free(name);

		ctx.current_inode = entry->inode;
		ctx.block = entry->block;
		ctx.parent = entry->parent;
		ctx.hash = entry->hindex;
		check_inode(&ctx);

		dirscan_release_entry(entry);
	}
	dirscan_end(scan);

	// TODO check all set bits for real inodes.  If they are there,
	// move them to root directory with a found-<name>
	if (ctx.bitmap) 
		free(ctx.bitmap);
	return 1;
}
