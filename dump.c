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

static int on_node(dirscan_t *d, dirscan_entry_t *entry, void *user)
{
	char *name = escape(entry->inode->i_name);

	printf("inode: %*c%s%c s:%" PRIx64 " h:%d c:%d p:%" PRIx64
           " b:%" PRIx64 "\n",
		entry->level*2, ' ', name,
		(entry->inode->i_type == OMFS_DIR) ? '/' : ' ',
		swap_be64(entry->inode->i_head.h_self), entry->hindex,
		swap_be16(entry->inode->i_head.h_crc), 
		entry->parent, entry->block); 
	free(name);

	return 0;
}

int dump_fs(FILE *fp)
{
	int bsize;
	omfs_super_t super;
	omfs_root_t root;
	omfs_info_t info = { 
		.dev = fp, 
		.super = &super,
		.root = &root
	};

	if (omfs_read_super(&info))
	{
		printf ("Could not read super block\n");
		return 0;
	}
	printf("Filesystem volume name: %s\n", super.s_name);
	printf("Filesystem magic number: 0x%x\n", swap_be32(super.s_magic));
	printf("First block: 0x%" PRIx64 "\n", swap_be64(super.s_root_block));
	printf("Block count: 0x%" PRIx64 "\n", swap_be64(super.s_num_blocks));
	printf("Block size: %d\n", swap_be32(super.s_blocksize));
	printf("Inode block size: %d\n", swap_be32(super.s_sys_blocksize));
	printf("Mirrors: %d\n", swap_be32(super.s_mirrors));
	putchar('\n');

	if (omfs_read_root_block(&info))
	{
		printf ("Could not read root block\n");
		return 0;
	}
	printf("Root block size: %d\n", swap_be32(info.root->r_blocksize));
	printf("Cluster size: %d\n", swap_be32(info.root->r_clustersize));
	printf("Root mirrors: %d\n", swap_be32(info.root->r_mirrors));

	bsize = (swap_be64(info.super->s_num_blocks) + 7) / 8;

	if (dirscan_begin(&info, on_node, NULL) != 0)
	{
		printf("Dirscan failed\n");
		return 0;
	}
	return 1;
}
