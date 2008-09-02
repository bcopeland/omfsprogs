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
	"Block bitmap is inconsistent",
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
	"Directory scan failed",
	"Loop detected for block $B"
};

static void hack_exit(check_context_t *ctx)
{
	printf("Made changes; re-run omfsck to continue scan\n");
	fclose(ctx->omfs_info->dev);
	exit(1);
}

// returns inode containing current file
omfs_inode_t *find_node(check_context_t *ctx, int *is_parent)
{
	omfs_inode_t *parent, *inode = 
		omfs_get_inode(ctx->omfs_info, ctx->parent);

	u64 *chain_ptr  = (u64*) ((u8*) inode + OMFS_DIR_START);

	parent = inode;
	if (!inode)
		return NULL;

	chain_ptr += ctx->hash;
	while (*chain_ptr != swap_be64(ctx->block) && *chain_ptr != ~0)
	{
		omfs_release_inode(inode);
		inode = omfs_get_inode(ctx->omfs_info, swap_be64(*chain_ptr));
		chain_ptr = &inode->i_sibling;
	}

	if (*chain_ptr == ~0)
	{
		omfs_release_inode(inode);
		return NULL;
	}
	*is_parent = parent == inode;
	return inode;
}

static u64 *get_entry(struct omfs_inode *inode, int hash, int is_parent)
{
	u64 *entry;
	if (is_parent)
		entry = (u64 *) ((u8 *) inode + OMFS_DIR_START) + hash;
	else
		entry = &inode->i_sibling;
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
	*entry = ctx->current_inode->i_sibling;
	res = omfs_write_inode(ctx->omfs_info, source);
	omfs_release_inode(source);
	if (res)
		perror("omfsck");

	dest = omfs_get_inode(ctx->omfs_info, dest_dir);
	if (dest->i_type != OMFS_DIR)
	{
		printf("Huh, tried to move it to a non-dir.. oh well.\n");
		return;
	}
	hash = omfs_compute_hash(ctx->omfs_info, ctx->current_inode->i_name);
	entry = get_entry(dest, hash, 1);
	ctx->current_inode->i_sibling = *entry;
	*entry = swap_be64(ctx->block);
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
	if (ctx->config->is_quiet)
		return;

	sad_print(output_strings[error], ctx);

	switch (error)
	{
	case E_SELF_PTR:
	case E_PARENT_PTR:
	case E_LOOP:
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
		if (prompt_yesno("Correct?"))
		{
			printf("Okay, fixing.\n");
			// write recomputes checksums
			omfs_write_inode(ctx->omfs_info, ctx->current_inode);
			omfs_sync(ctx->omfs_info);
		}
		else
			printf("Skipping.\n");
		break;
	case E_HASH_WRONG:
		if (prompt_yesno("Correct?"))
		{
			printf("Okay, moving to proper location.\n");
			move_file(ctx, ctx->parent);
		}
		else
			printf("Skipping.\n");
		break;
	case E_BITMAP:
		if (prompt_yesno("Rebuild?"))
		{
			printf("Okay writing computed bitmap.\n");
			ctx->omfs_info->bitmap->bmap = ctx->visited;
			omfs_flush_bitmap(ctx->omfs_info);
		}
		else
			printf("Skipping.\n");
		break;
	default:
		printf("Weird, I don't do anything about that yet\n");
		break;
	}
}
