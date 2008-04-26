#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "omfs.h"
#include "bits.h"
#include "config.h"

void make_bad_hash(FILE *fp)
{
	int i;
	u64 block;
	omfs_super_t super;
	omfs_root_t root;
	omfs_info_t info = { 
		.dev = fp, 
		.super = &super,
		.root = &root
	};

	omfs_read_super(fp, &super);
	omfs_read_root_block(fp, &super, &root);
	u8 *bitmap = omfs_get_bitmap(&info);

	for (i=0; i<super.num_blocks * 8; i++)
	{
		if (!test_bit(bitmap, (u64) i))
		{
			set_bit(bitmap, i);
			set_bit(bitmap, i+1);
			break;
		}
	}
	block = i;
	omfs_write_bitmap(&info, bitmap);

	omfs_inode_t ino = {
		.head.self = swap_be64(block),
		.head.body_size = swap_be32(swap_be32(super.sys_blocksize) - 
				sizeof(omfs_header_t)),
		.head.version = 1,
		.head.type = OMFS_INODE_NORMAL,
		.head.magic = OMFS_IMAGIC,
		.parent = root.root_dir,
		.sibling = ~0,
		.ctime = 0,
		.type = 'D',
		.one_goes_here = swap_be32(1),
		.name = "bad_hash",
	};

	u8 *data = calloc(1, swap_be32(super.sys_blocksize));
	if (!data) 
		return;

	memcpy(data, &ino, sizeof (omfs_inode_t));
	memset(data + OMFS_DIR_START, 0xff, 
		swap_be32(super.sys_blocksize) - OMFS_DIR_START);
	omfs_write_inode(&info, (omfs_inode_t *) data);

	int hash = omfs_compute_hash(&info, ino.name);
	hash++;

	omfs_inode_t *root_dir = omfs_get_inode(&info, 
			swap_be64(root.root_dir)); 
	u64 *entry = (u64*) ((u8*)root_dir + OMFS_DIR_START + hash * 8);
	*entry = ino.head.self;
	omfs_write_inode(&info, root_dir);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 1;

	FILE *fp = fopen(argv[1], "r+");

	make_bad_hash(fp);
	fclose(fp);
	return 0;
}

