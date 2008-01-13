/* 
 *  Here is where we supposedly fix things.
 */

#include <stdlib.h>
#include "omfs.h"
#include "check.h"
#include "fix.h"
#include "io.h"

static char *output_strings[] = 
{
	"Hey, no error here!",
	"Header XOR for inode $I is incorrect",
	"Header CRC for inode $I is incorrect",
	"Inode $I is not linked in any directory",
	"In-use bit for inode $I should be set",
	"File $F is in wrong hash index $H",
	"Blocksize is unheard of",
	"System blocksize is invalid",
	"Mirror count is clearly wrong",
	"Extent count for $I is out of range",
	"Extent terminator for inode $I checksum doesn't add up",
	"Filesystem magic number is wrong (is this really an OMFS partition?)",
	"Inode magic number is wrong for $I",
	"Self pointer on $I is wrong",
	"Parent pointer on $I is wrong",
	"Could not read super block",
	"Could not read root block",
	"Inode $I is totally busted",
	"Directory scan failed"
};

static void hack_exit(check_context_t *ctx)
{
	printf("Made changes; re-run omfsck to continue scan\n");
	fclose(ctx->omfs_info->dev);
	exit(1);
}

static u64 swap_block(u64 block, int doswap)
{
	// yuck!  swap if we're storing a sibling ptr
	// rewrite omfs.c so this crap doesn't happen.
	return (doswap) ? block : swap64(block);
}

// returns inode containing current file
omfs_inode_t *find_node(check_context_t *ctx, int *is_parent)
{
	omfs_inode_t *parent, *inode = 
		omfs_get_inode(ctx->omfs_info, ctx->parent);

	parent = inode;
	if (!inode)
		return NULL;

	u64 *chain_ptr;
	chain_ptr = ((u64*) &inode->data[OMFS_DIR_START]) + ctx->hash;
	while (swap_block(*chain_ptr, parent != inode) != ctx->block && 
		*chain_ptr != ~0)
	{
		omfs_release_inode(inode);
		inode = omfs_get_inode(ctx->omfs_info, swap_be64(*chain_ptr));
		chain_ptr = &inode->sibling;
	}

	if (*chain_ptr == ~0)
	{
		omfs_release_inode(inode);
		return NULL;
	}
	return inode;
}

static u64 *get_entry(struct omfs_inode *inode, int hash, int is_parent)
{
	u64 *entry;
	if (is_parent)
		entry = ((u64 *) &inode->data[OMFS_DIR_START]) + hash;
	else
		entry = &inode->sibling;
	return entry;
}

static void delete_file(check_context_t *ctx)
{
	int res;
	int is_parent;
	u64 *entry;
	omfs_inode_t *inode = find_node(ctx, &is_parent);

	if (!inode)
	{
		fprintf(stderr, "Oops, didn't find it.  Odd.\n");
		return;
	}
	entry = get_entry(inode, ctx->hash, is_parent);
	*entry = ~0;
	res = omfs_write_inode(ctx->omfs_info, inode);
	if (res)
		perror("omfsck");

	omfs_release_inode(inode);
	omfs_sync(ctx->omfs_info);

	// force clear bit later when we consistency-check the bitmap.
	hack_exit(ctx);
}

// can be used to fix hash bugs or move around FS
static void move_file(check_context_t *ctx, u64 dest_dir)
{
	omfs_inode_t *source;
	omfs_inode_t *dest;
	u64 *entry;
	int is_parent, res;
	int hash;

	source = find_node(ctx, &is_parent);
	if (!source)
	{
		fprintf(stderr, "Oops, didn't find it.  Odd.\n");
		return;
	}
	entry = get_entry(source, ctx->hash, is_parent);
	*entry = swap_block(ctx->current_inode->sibling, is_parent);
	res = omfs_write_inode(ctx->omfs_info, source);
	omfs_release_inode(source);
	if (res)
		perror("omfsck");

	dest = omfs_get_inode(ctx->omfs_info, dest_dir);
	if (dest->type != OMFS_DIR)
	{
		printf("Huh, tried to move it to a non-dir.. oh well.\n");
		return;
	}
	hash = omfs_compute_hash(ctx->omfs_info, ctx->current_inode->name);
	entry = get_entry(dest, hash, 1);
	ctx->current_inode->sibling = swap_block(*entry, 1);
	*entry = swap64(ctx->block);
	res = omfs_write_inode(ctx->omfs_info, dest);
	if (res)
		perror("omfsck");
	res = omfs_write_inode(ctx->omfs_info, ctx->current_inode);
	if (res)
		perror("omfsck");

	omfs_release_inode(ctx->current_inode);
	omfs_release_inode(dest);
	omfs_sync(ctx->omfs_info);

	// force clear bit later when we consistency-check the bitmap.
	hack_exit(ctx);
}

void fix_problem(check_error_t error, check_context_t *ctx)
{
	sad_print(output_strings[error], ctx);

	switch (error)
	{
	case E_BIT_CLEAR:
	case E_SELF_PTR:
	case E_PARENT_PTR:
	case E_INSANE:
		/* for now, a take no prisoners approach */
		if (prompt_yesno("Delete the offending file?"))
		{
			printf("Okay, deleting.\n");
			delete_file(ctx);
		}
		else
			printf("Skipping.\n");
		break;
	case E_HEADER_XOR:
	case E_HEADER_CRC:
		printf("Fixing.\n");
		// write recomputes checksums
		omfs_write_inode(ctx->omfs_info, ctx->current_inode);
		omfs_sync(ctx->omfs_info);
		break;
	case E_HASH_WRONG:
		printf("Fixing.\n");
		move_file(ctx, ctx->parent);
	default:
		printf("Weird, I don't do anything about that yet\n");
		break;
	}
}
