/*
 * dirscan.c - iterator for traversing the directory tree.  
 *
 * We never have to hold more than a few inodes in memory.  Let 
 * the OS cache em.
 *
 * TODO: add cycle detection
 */

#include <stdlib.h>
#include "dirscan.h"

static dirscan_entry_t *_create_entry(omfs_inode_t *inode, 
		int level, int hindex, u64 parent, u64 block)
{
	dirscan_entry_t *entry = malloc(sizeof(dirscan_entry_t));
	entry->inode = inode;
	entry->level = level;
	entry->hindex = hindex;
	entry->parent = parent;
	entry->block = block;

	return entry;
}

static void dirscan_release_entry(dirscan_entry_t *entry)
{
	omfs_release_inode(entry->inode);
	free(entry);
}


int traverse (dirscan_t *d, dirscan_entry_t *entry)
{
	omfs_inode_t *ino, *tmp;
	dirscan_entry_t *enew;

	int res = d->visit(d, entry, d->user_data);

	ino = entry->inode;
	
	/* push next sibling all then all children */
	if (ino->sibling != ~0)
	{
		tmp = omfs_get_inode(d->omfs_info, swap_be64(ino->sibling));
		if (!tmp) 
		{
			res = -1;
			goto out;
		}

		enew = _create_entry(tmp, entry->level, entry->hindex,
				entry->parent, swap_be64(ino->sibling));
		traverse(d, enew);
	}
	if (ino->type == OMFS_DIR)
	{
		int i;
		u64 *ptr = (u64*) ((u8*) ino + OMFS_DIR_START);

		int num_entries = (swap_be32(ino->head.body_size) + 
			sizeof(omfs_header_t) - OMFS_DIR_START) / 8;

		for (i=0; i<num_entries; i++, ptr++)
		{
			u64 inum = swap_be64(*ptr);
			if (inum != ~0)
			{
				tmp = omfs_get_inode(d->omfs_info, inum);
				if (!tmp) {
					res = -1;
					goto out;
				}
				enew = _create_entry(tmp, entry->level+1, i,
					entry->block, inum);
				traverse(d, enew);
			}
		}
	}
out:
	dirscan_release_entry(entry);
	return res;
}

static void dirscan_end(dirscan_t *d)
{
	free(d);
}

int dirscan_begin(omfs_info_t *info, int (*visit)(dirscan_t *, 
			dirscan_entry_t*, void*), void *user_data) 
{
	omfs_inode_t *root_ino;
	int res;

	dirscan_t *d = calloc(sizeof(dirscan_t), 1);
	if (!d)
		goto error;

	d->omfs_info = info;
	d->visit = visit;
	d->user_data = user_data;

	root_ino = omfs_get_inode(info, swap_be64(info->root->root_dir));
	if (!root_ino)
		goto error;
	res = traverse(d, _create_entry(root_ino, 0, 0, ~0, 
			swap_be64(info->root->root_dir)));
	dirscan_end(d);

	return 0;

error:
	if (d) free(d);
	return -1;
}

