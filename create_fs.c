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
		.root_block = swap_be64(ROOT_BLK),
		.num_blocks = swap_be64(blocks),
		.magic = swap_be32(OMFS_MAGIC),
		.blocksize = swap_be32(block_size),
		.mirrors = swap_be32(2),
		.sys_blocksize = swap_be32(block_size/4), // ??
	};

	omfs_root_t root = 
	{
		.head.self = swap_be64(ROOT_BLK),
		.head.body_size = swap_be32(sizeof(omfs_root_t) - sizeof(struct omfs_header)),
		.head.version = 1,
		.head.type = OMFS_INODE_SYSTEM,
		.head.magic = OMFS_IMAGIC,
		.num_blocks = super.num_blocks,
		.root_dir = swap_be64(ROOT_DIR_BLK),
		.bitmap = swap_be64(BITMAP_BLK),
		.blocksize = super.blocksize,
		.clustersize = swap_be32(config->cluster_size),
		.mirrors = swap_be64(super.mirrors),
	};

	safe_strncpy(super.name, label, OMFS_SUPER_NAMELEN);
	safe_strncpy(root.name, label, OMFS_NAMELEN);

	// super block
	omfs_write_super(fp, &super);
	omfs_write_root_block(fp, &super, &root);

	u64 now = time(NULL) * 1000LL;

	// root directory
	omfs_inode_t root_ino = {
		.head.self = swap_be64(ROOT_DIR_BLK),
		.head.body_size = swap_be32(swap_be32(super.sys_blocksize) - sizeof(omfs_header_t)),
		.head.version = 1,
		.head.type = OMFS_INODE_NORMAL,
		.head.magic = OMFS_IMAGIC,
		.parent = ~0,
		.sibling = ~0,
		.ctime = swap_be64(now),
		.type = 'D',
		.one_goes_here = swap_be32(1),
		.size = swap_be64(swap_be32(super.sys_blocksize))
	};

	u8 *data = calloc(1, swap_be32(super.sys_blocksize));
	if (!data) 
		return 0;

	memcpy(data, &root_ino, sizeof (omfs_inode_t));
	memset(data + OMFS_DIR_START, 0xff, 
		swap_be32(super.sys_blocksize) - OMFS_DIR_START);

	info.dev = fp;
	info.super = &super;
	info.root = &root;
	omfs_write_inode(&info, (omfs_inode_t *) data);

	free(data);
		
	// free space bitmap.  We already know that blocks 0-5 are 
	// going to be set; the first available cluster has to take
	// into account the size of the free space bitmap.
	int bitmap_size = (swap_be64(super.num_blocks) + 7)/8;
	int first_blk = BITMAP_BLK + (bitmap_size + 
		swap_be32(super.blocksize)-1) / swap_be32(super.blocksize);

	u8 *bitmap = calloc(1, bitmap_size);

	for (i=0; i<first_blk; i++)
	{
		bitmap[i/8] |= 1<<(i & 7);
	}
	omfs_write_bitmap(&info, bitmap);
	
	return 1;
}
