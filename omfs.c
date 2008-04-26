/*
 *  Super/root block reading routines
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "omfs.h"
#include "crc.h"

static void _omfs_swap_buffer(void *buf, int count)
{
#ifdef RTV_HACK
    // only valid for Replay 4xxx and 5xxx: unconditionally force change 
    // of endianness
    int i;
	u32 *ibuf = (u32 *) buf;
	count >>= 2;

	for (i=0; i<count; i++)
		ibuf[i] = __swap32(ibuf[i]);
#endif
}

/*
 * Write the superblock to disk
 */
int omfs_write_super(FILE *dev, struct omfs_super_block *super)
{
	int count;

	fseeko(dev, 0LL, SEEK_SET);
	_omfs_swap_buffer(super, sizeof(struct omfs_super_block));
	count = fwrite(super, 1, sizeof(struct omfs_super_block), dev);

	if (count < sizeof(struct omfs_super_block))
		return -1;

	return 0;
}

/*
 * Read the superblock and store the result in ret
 */
int omfs_read_super(FILE *dev, struct omfs_super_block *ret)
{
	int count;

	fseeko(dev, 0LL, SEEK_SET);
	count = fread(ret, 1, sizeof(struct omfs_super_block), dev);

	if (count < sizeof(struct omfs_super_block))
		return -1;

	_omfs_swap_buffer(ret, count);
	return 0;
}

static int _omfs_write_block(FILE *dev, struct omfs_super_block *sb,
		u64 block, u8* buf, size_t len)
{
	int i, count;
	_omfs_swap_buffer(buf, len);
	for (i=0; i<swap_be32(sb->mirrors); i++)
	{
	    fseeko(dev, (block + i) * swap_be32(sb->blocksize), SEEK_SET);
	    count = fwrite(buf, 1, len, dev);
	    if (count != len)
		    return -1;
	}
	return 0;
}


/*
 * Read the numbered block into a raw array.
 * buf must be at least blocksize bytes.
 */
static int _omfs_read_block(FILE *dev, struct omfs_super_block *sb, 
				u64 block, u8 *buf)
{
	int count;
	fseeko(dev, block * swap_be32(sb->blocksize), SEEK_SET);
	count = fread(buf, 1, swap_be32(sb->blocksize), dev);

	if (count < swap_be32(sb->blocksize))
		return -1;

	_omfs_swap_buffer(buf, count);
	return 0;
}

static void _update_header_checksums(u8 *buf, int block_size) 
{
	int xor, i;
	omfs_header_t *header = (omfs_header_t *) buf;
	u8 *body = buf + sizeof(omfs_header_t);
	int body_size = block_size - sizeof(omfs_header_t);

	header->crc = swap_be16(crc_ccitt_msb(0, body, body_size));

	xor = buf[0];
	for (i=1; i<OMFS_XOR_COUNT; i++)
		xor ^= buf[i];
	header->check_xor = xor;
}


int omfs_write_root_block(FILE *dev, struct omfs_super_block *sb,
		struct omfs_root_block *root)
{
	u64 block = swap_be64(root->head.self);
	return _omfs_write_block(dev, sb, block, (u8*) root, 
			sizeof(struct omfs_root_block));
}


int omfs_read_root_block(FILE *dev, struct omfs_super_block *sb, 
		struct omfs_root_block *root)
{
	u8 *buf;

	buf = omfs_get_block(dev, sb, swap_be64(sb->root_block));
	if (!buf)
		return -1;

	memcpy(root, buf, sizeof(struct omfs_root_block));
	free(buf);
	return 0;
}

u8 *omfs_get_block(FILE *dev, struct omfs_super_block *sb, u64 block)
{
	u8 *buf;
	if (!(buf = malloc(swap_be32(sb->blocksize))))
		return 0;

	if (_omfs_read_block(dev, sb, block, buf))
	{
		free(buf);
		return 0;
	}

	return buf;
}

/*
 *  Write an inode to the device.
 */
int omfs_write_inode(omfs_info_t *info, omfs_inode_t *inode)
{
	int size = swap_be32(inode->head.body_size) + sizeof(omfs_header_t);

	_update_header_checksums((u8*)inode, size);

	return _omfs_write_block(info->dev, info->super, 
			swap_be64(inode->head.self), (u8*) inode, size);
}

omfs_inode_t *omfs_get_inode(omfs_info_t *info, u64 block)
{
	u8 *buf;
	buf = omfs_get_block(info->dev, info->super, block);
	if (!buf)
		return NULL;

	return (omfs_inode_t *) buf;
}

void omfs_release_inode(omfs_inode_t *oi)
{
	free(oi);
}

int omfs_write_bitmap(omfs_info_t *info, u8* bitmap)
{
	size_t size, count;
	u64 bitmap_blk = swap_be64(info->root->bitmap);

	size = (swap_be64(info->super->num_blocks) + 7) / 8;
	fseeko(info->dev, bitmap_blk * swap_be32(info->super->blocksize), 
			SEEK_SET);
	count = fwrite(bitmap, 1, size, info->dev);
	if (size != count)
		return -1;
	return 0;
}

u8 *omfs_get_bitmap(omfs_info_t *info)
{
	size_t size;
	u8 *buf;
	u64 bitmap_blk = swap_be64(info->root->bitmap);
	if (bitmap_blk == ~0)
		return NULL;

	size = (swap_be64(info->super->num_blocks) + 7) / 8;

	if (!(buf = malloc(size)))
		return NULL;

	fseeko(info->dev, bitmap_blk * swap_be32(info->super->blocksize), SEEK_SET);
	fread(buf, 1, size, info->dev);
	return buf;
}

int omfs_compute_hash(omfs_info_t *info, char *filename)
{
	int hash = 0, i;
	int m = (swap_be32(info->super->sys_blocksize) - OMFS_DIR_START) / 8;
	
	for (i=0; i<strlen(filename); i++)
		hash ^= tolower(filename[i]) << (i % 24);
	
	return hash % m;
}

void omfs_sync(omfs_info_t *info)
{
	fflush(info->dev);
}
