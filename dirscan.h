#ifndef _DIRSCAN_H
#define _DIRSCAN_H

#include "stack.h"
#include "omfs.h"

struct dirscan
{
	omfs_info_t *omfs_info;    /* omfs lib context */
	stack_t *inode_stack;      /* where we are in the traversal */
}; 

struct dirscan_entry
{
	omfs_inode_t *inode;       /* an inode */
	int level;                 /* level in the tree */
	int hindex;                /* hash index */
	u64 parent;                /* parent inode number */
	u64 block;                 /* block from which inode was read */
};

typedef struct dirscan dirscan_t;
typedef struct dirscan_entry dirscan_entry_t;

dirscan_t *dirscan_begin(omfs_info_t *info);
dirscan_entry_t *dirscan_next(dirscan_t *d);
void dirscan_release_entry(dirscan_entry_t *entry);
void dirscan_end(dirscan_t *d);

#endif
