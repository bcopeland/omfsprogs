#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <time.h>
#include <string.h>

#include "omfs.h"
#include "create_fs.h"
#include "crc.h"

#define ROOT_BLK 1
#define ROOT_DIR_BLK 3
#define BITMAP_BLK 5
#define SECTOR_SIZE 512

void safe_strncpy(char *dest, char *src, int dest_size)
{
	strncpy(dest, src, dest_size-1);
	dest[dest_size-1] = 0;
}

void clear_dev(FILE *fp, u64 sectors)
{
	int i; 
	char blk[SECTOR_SIZE];

	// clear the device
	 
	memset(blk, 0, sizeof(blk));
	for (i=0; i<sectors; i++)
	{
		fwrite(blk, 1, sizeof(blk), fp);
	}
	rewind(fp);
}

int create_fs(FILE *fp, u64 sectors, fs_config_t *config)
{
	int i;
	int block_size = config->block_size;
	char *label = "omfs";
	omfs_info_t info;

	int blocks_per_sector = block_size / SECTOR_SIZE;
	int blocks = sectors / blocks_per_sector;

	if (config->clear_dev)
		clear_dev(fp, sectors);

	omfs_super_t super = 
	{
		.s_root_block = swap_be64(ROOT_BLK),
		.s_num_blocks = swap_be64(blocks),
		.s_magic = swap_be32(OMFS_MAGIC),
		.s_blocksize = swap_be32(block_size),
		.s_mirrors = swap_be32(2),
		.s_sys_blocksize = swap_be32(2048), 
	};

	omfs_root_t root = 
	{
		.r_head.h_self = swap_be64(ROOT_BLK),
		.r_head.h_body_size = swap_be32(sizeof(omfs_root_t) - sizeof(struct omfs_header)),
		.r_head.h_version = 1,
		.r_head.h_type = OMFS_INODE_SYSTEM,
		.r_head.h_magic = OMFS_IMAGIC,
		.r_num_blocks = super.s_num_blocks,
		.r_root_dir = swap_be64(ROOT_DIR_BLK),
		.r_bitmap = swap_be64(BITMAP_BLK),
		.r_blocksize = super.s_blocksize,
		.r_clustersize = swap_be32(config->cluster_size),
		.r_mirrors = swap_be64(super.s_mirrors),
	};

	omfs_bitmap_t bitmap;

	safe_strncpy(super.s_name, label, OMFS_SUPER_NAMELEN);
	safe_strncpy(root.r_name, label, OMFS_NAMELEN);

	info.dev = fp;
	info.super = &super;
	info.root = &root;
	info.bitmap = &bitmap;

	// super block
	omfs_write_super(&info);
	omfs_write_root_block(&info);

	u64 now = time(NULL) * 1000LL;

	// root directory
	omfs_inode_t root_ino = {
		.i_head.h_self = swap_be64(ROOT_DIR_BLK),
		.i_head.h_body_size = swap_be32(swap_be32(super.s_sys_blocksize) - sizeof(omfs_header_t)),
		.i_head.h_version = 1,
		.i_head.h_type = OMFS_INODE_NORMAL,
		.i_head.h_magic = OMFS_IMAGIC,
		.i_parent = ~0,
		.i_sibling = ~0,
		.i_ctime = swap_be64(now),
		.i_type = 'D',
		.i_fill2 = swap_be32(1),
		.i_size = swap_be64(swap_be32(super.s_sys_blocksize))
	};

	u8 *data = calloc(1, swap_be32(super.s_sys_blocksize));
	if (!data) 
		return 0;

	memcpy(data, &root_ino, sizeof (omfs_inode_t));
	memset(data + OMFS_DIR_START, 0xff, 
		swap_be32(super.s_sys_blocksize) - OMFS_DIR_START);

	omfs_write_inode(&info, (omfs_inode_t *) data);

	free(data);
		
	// free space bitmap.  We already know that blocks 0-5 are 
	// going to be set; the first available cluster has to take
	// into account the size of the free space bitmap.
	int bitmap_size = (swap_be64(super.s_num_blocks) + 7)/8;
	int first_blk = BITMAP_BLK + (bitmap_size + 
		swap_be32(super.s_blocksize)-1) / swap_be32(super.s_blocksize);

	bitmap.bmap = calloc(1, bitmap_size);

	for (i=0; i<first_blk; i++)
	{
		bitmap.bmap[i/8] |= 1<<(i & 7);
	}
	omfs_flush_bitmap(&info);
	
	return 1;
}
