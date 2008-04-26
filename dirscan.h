#ifndef _DIRSCAN_H
#define _DIRSCAN_H

#include "stack.h"
#include "omfs.h"

struct dirscan_entry
{
	omfs_inode_t *inode;       /* an inode */
	int level;                 /* level in the tree */
	int hindex;                /* hash index */
	u64 parent;                /* parent inode number */
	u64 block;                 /* block from which inode was read */
};

struct dirscan
{
	omfs_info_t *omfs_info;    /* omfs lib context */
	int (*visit) (struct dirscan *, struct dirscan_entry *, void*);
	void *user_data;
	int visit_error;
}; 

typedef struct dirscan dirscan_t;
typedef struct dirscan_entry dirscan_entry_t;

int dirscan_begin(omfs_info_t *info, int (*visit)(dirscan_t *, 
			dirscan_entry_t*, void*), void *user_data);

#endif
