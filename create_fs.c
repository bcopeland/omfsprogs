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
	 
	memset(blk, 'z', sizeof(blk));
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
		.root_block = ROOT_BLK,
		.num_blocks = blocks,
		.magic = OMFS_MAGIC,
		.blocksize = block_size,
		.mirrors = 2,
		.sys_blocksize = block_size/4, // ??
	};

	omfs_root_t root = 
	{
		.head.self = ROOT_BLK,
		.head.body_size = sizeof(omfs_root_t) - sizeof(struct omfs_header),
		.head.version = 1,
		.head.type = OMFS_INODE_SYSTEM,
		.head.magic = OMFS_IMAGIC,
		.num_blocks = super.num_blocks,
		.root_dir = ROOT_DIR_BLK,
		.bitmap = BITMAP_BLK,
		.blocksize = super.blocksize,
		.clustersize = config->cluster_size,
		.mirrors = super.mirrors,
	};

	safe_strncpy(super.name, label, OMFS_SUPER_NAMELEN);
	safe_strncpy(root.name, label, OMFS_NAMELEN);

	// super block
	omfs_write_super(fp, &super);
	omfs_write_root_block(fp, &super, &root);

	u64 now = time(NULL) * 1000LL;

	// root directory
	omfs_inode_t root_ino = {
		.head.self = ROOT_DIR_BLK,
		.head.body_size = super.sys_blocksize - sizeof(omfs_header_t),
		.head.version = 1,
		.head.type = OMFS_INODE_NORMAL,
		.head.magic = OMFS_IMAGIC,
		.parent = ~0,
		.sibling = ~0,
		.ctime = now,
		.type = 'D',
		.one_goes_here = 1
	};

	u8 *data = calloc(1, super.sys_blocksize);
	if (!data) 
		return 0;

	memset(data + OMFS_DIR_START, 0xff, 
		super.sys_blocksize - OMFS_DIR_START);

	root_ino.data = data;

	info.dev = fp;
	info.super = &super;
	info.root = &root;
	omfs_write_inode(&info, &root_ino);
		
	// free space bitmap.  We already know that blocks 0-5 are 
	// going to be set; the first available cluster has to take
	// into account the size of the free space bitmap.
	int bitmap_size = (super.num_blocks + 7)/8;
	int first_blk = BITMAP_BLK + (bitmap_size + super.blocksize-1) /
		super.blocksize;

	u8 *bitmap = calloc(1, bitmap_size);

	for (i=0; i<first_blk; i++)
	{
		bitmap[i/8] |= 1<<(i & 7);
	}
	omfs_write_bitmap(&info, bitmap);
	
	return 1;
}
