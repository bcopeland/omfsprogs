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

dirscan_t *dirscan_begin(omfs_info_t *info)
{
	omfs_inode_t *root_ino;

	dirscan_t *d = calloc(sizeof(dirscan_t), 1);
	if (!d)
		goto error;
	d->inode_stack = stack_init();
	if (!d->inode_stack) 
		goto error;

	root_ino = omfs_get_inode(info, info->root->root_dir);
	if (!root_ino)
		goto error;

	stack_push(d->inode_stack, _create_entry(root_ino, 0, 0, ~0, 
		info->root->root_dir));

	d->omfs_info = info;
	return d;

error:
	if (d->inode_stack) stack_destroy(d->inode_stack);
	if (d) free(d);
	return NULL;
}


/*
 *  Returns the next inode in the tree.  This is a stack-based 
 *  traversal, because traversing it like a tree and calling a 
 *  callback would be too easy.
 *
 *  Returns NULL when no more inodes are left.  The caller should 
 *  call dirscan_release_entry on the returned dirscan_entry.
 */
dirscan_entry_t *dirscan_next(dirscan_t *d)
{
	omfs_inode_t *ino, *tmp;
	dirscan_entry_t *entry, *enew;

	if (stack_empty(d->inode_stack))
	{
		return NULL;
	}

	entry = stack_pop(d->inode_stack);
	ino = entry->inode;
	
	/* push next sibling all then all children */
	if (ino->sibling != ~0)
	{
		tmp = omfs_get_inode(d->omfs_info, ino->sibling);
		if (!tmp) return NULL;

		enew = _create_entry(tmp, entry->level, entry->hindex,
				entry->parent, ino->sibling);
		stack_push(d->inode_stack, enew);
	}
	if (ino->type == OMFS_DIR)
	{
		int i;
		u64 *ptr = (u64*) &ino->data[OMFS_DIR_START];

		int num_entries = (ino->head.body_size + 
			sizeof(omfs_header_t) - OMFS_DIR_START) / 8;

		for (i=0; i<num_entries; i++, ptr++)
		{
			u64 inum = swap_be64(*ptr);
			if (inum != ~0)
			{
				tmp = omfs_get_inode(d->omfs_info, inum);
				if (!tmp) return NULL;
				enew = _create_entry(tmp, entry->level+1, i,
					entry->block, inum);
				stack_push(d->inode_stack, enew);
			}
		}
	}
	return entry;
}

void dirscan_release_entry(dirscan_entry_t *entry)
{
	omfs_release_inode(entry->inode);
	free(entry);
}

void dirscan_end(dirscan_t *d)
{
	while (!stack_empty(d->inode_stack))
	{
		dirscan_release_entry(stack_pop(d->inode_stack));
	}
	stack_destroy(d->inode_stack);
	free(d);
}

