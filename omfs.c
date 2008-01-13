/*
 *  Super/root block reading routines
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "omfs.h"
#include "crc.h"

/*
 * Write the superblock to disk (with endian conversions)
 */
int omfs_write_super(FILE *dev, struct omfs_super_block *super)
{
	struct omfs_super_block tmp;
	int count;

	memcpy(&tmp, super, sizeof(omfs_super_t));

	/* fix endianness */
	tmp.root_block = swap_be64(tmp.root_block);
	tmp.num_blocks = swap_be64(tmp.num_blocks);
	tmp.magic = swap_be32(tmp.magic);
	tmp.blocksize = swap_be32(tmp.blocksize);
	tmp.mirrors = swap_be32(tmp.mirrors);
	tmp.sys_blocksize = swap_be32(tmp.sys_blocksize);

	fseeko(dev, 0LL, SEEK_SET);
	count = fwrite(&tmp, 1, sizeof(struct omfs_super_block), dev);

	if (count < sizeof(struct omfs_super_block))
		return -1;

	return 0;
}

/*
 * Read the superblock and store the result in ret (with endian conversions)
 */
int omfs_read_super(FILE *dev, struct omfs_super_block *ret)
{
	int count;

	fseeko(dev, 0LL, SEEK_SET);
	count = fread(ret, 1, sizeof(struct omfs_super_block), dev);

	if (count < sizeof(struct omfs_super_block))
		return -1;

	/* fix endianness */
	ret->root_block = swap_be64(ret->root_block);
	ret->num_blocks = swap_be64(ret->num_blocks);
	ret->magic = swap_be32(ret->magic);
	ret->blocksize = swap_be32(ret->blocksize);
	ret->mirrors = swap_be32(ret->mirrors);
	ret->sys_blocksize = swap_be32(ret->sys_blocksize);

	return 0;
}

static void fix_header_endian(struct omfs_header *h)
{
	h->self = swap_be64(h->self);
	h->body_size = swap_be32(h->body_size);
	h->crc = swap_be16(h->crc);
}

static int _omfs_write_block(FILE *dev, struct omfs_super_block *sb,
		u64 block, u8* buf, size_t len)
{
	int i, count;
	for (i=0; i<sb->mirrors; i++)
	{
	    fseeko(dev, (block + i) * sb->blocksize, SEEK_SET);
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
	fseeko(dev, block * sb->blocksize, SEEK_SET);
	count = fread(buf, 1, sb->blocksize, dev);

	if (count < sb->blocksize)
		return -1;

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
	struct omfs_root_block tmp;
	u64 block = root->head.self;

	memcpy(&tmp, root, sizeof(struct omfs_root_block));

	fix_header_endian(&tmp.head);

	tmp.num_blocks = swap_be64(tmp.num_blocks);
	tmp.root_dir = swap_be64(tmp.root_dir);
	tmp.bitmap = swap_be64(tmp.bitmap);
	tmp.blocksize = swap_be32(tmp.blocksize);
	tmp.clustersize = swap_be32(tmp.clustersize);
	tmp.mirrors = swap_be64(tmp.mirrors);

	_update_header_checksums((u8*) &tmp, sizeof(struct omfs_root_block));

	return _omfs_write_block(dev, sb, block, (u8*) &tmp, 
			sizeof(struct omfs_root_block));
}


int omfs_read_root_block(FILE *dev, struct omfs_super_block *sb, 
		struct omfs_root_block *root)
{
	u8 *buf;

	buf = omfs_get_block(dev, sb, sb->root_block);
	if (!buf)
		return -1;

	memcpy(root, buf, sizeof(struct omfs_root_block));
	fix_header_endian(&root->head);

	root->num_blocks = swap_be64(root->num_blocks);
	root->root_dir = swap_be64(root->root_dir);
	root->bitmap = swap_be64(root->bitmap);
	root->blocksize = swap_be32(root->blocksize);
	root->clustersize = swap_be32(root->clustersize);
	root->mirrors = swap_be64(root->mirrors);

	free(buf);
	return 0;
}

u8 *omfs_get_block(FILE *dev, struct omfs_super_block *sb, u64 block)
{
	u8 *buf;
	if (!(buf = malloc(sb->blocksize)))
		return 0;

	if (_omfs_read_block(dev, sb, block, buf))
	{
		free(buf);
		return 0;
	}

	return buf;
}

/*
 *  Write an inode to the device.  The first 
 *  sizeof(omfs_inode_t) - sizeof(char*) bytes of the data pointer are 
 *  ignored as these map to the inode fields.  
 *
 *  FIXME: this is icky - sometimes we want a disk inode, sometimes 
 *  we want memory, and sometimes we want both.
 */
int omfs_write_inode(omfs_info_t *info, omfs_inode_t *inode)
{
	omfs_inode_t tmp;
	int inode_size;
	int total_size;
	u8 *buf;

	total_size = inode->head.body_size + sizeof(omfs_header_t);
	inode_size = sizeof(omfs_inode_t) - sizeof(char *);

	buf = malloc(total_size);
	memcpy(&tmp, inode, sizeof(omfs_inode_t));

	fix_header_endian(&tmp.head);
	tmp.parent = swap_be64(tmp.parent);
	tmp.sibling = swap_be64(tmp.sibling);
	tmp.ctime = swap_be64(tmp.ctime);
	tmp.size = swap_be64(tmp.size);
	tmp.one_goes_here = swap_be32(tmp.one_goes_here);

	memcpy(buf, &tmp, inode_size);
	memcpy(buf + inode_size, inode->data + inode_size,
			total_size - inode_size);

	_update_header_checksums(buf, inode->head.body_size + 
			sizeof(omfs_header_t));

	return _omfs_write_block(info->dev, info->super, inode->head.self, buf,
			inode->head.body_size + sizeof(omfs_header_t));
}

omfs_inode_t *omfs_get_inode(omfs_info_t *info, u64 block)
{
	u8 *buf;
	buf = omfs_get_block(info->dev, info->super, block);
	if (!buf)
		return NULL;

	struct omfs_inode *oi = calloc (1, sizeof(struct omfs_inode));
	if (!oi)
	{
		free(buf);
		return NULL;
	}

	memcpy(oi, buf, sizeof(struct omfs_inode));
	fix_header_endian(&oi->head);
	oi->parent = swap_be64(oi->parent);
	oi->sibling = swap_be64(oi->sibling);
	oi->ctime = swap_be64(oi->ctime);
	oi->size = swap_be64(oi->size);
	oi->data = buf;

	return oi;
}

void omfs_release_inode(omfs_inode_t *oi)
{
	free(oi->data);
	free(oi);
}

int omfs_write_bitmap(omfs_info_t *info, u8* bitmap)
{
	size_t size, count;
	u8 *buf;
	u64 bitmap_blk = info->root->bitmap;

	size = (info->super->num_blocks + 7) / 8;
	fseeko(info->dev, bitmap_blk * info->super->blocksize, SEEK_SET);
	count = fwrite(buf, 1, size, info->dev);
	if (size != count)
		return -1;
	return 0;
}

u8 *omfs_get_bitmap(omfs_info_t *info)
{
	size_t size;
	u8 *buf;
	u64 bitmap_blk = info->root->bitmap;
	if (bitmap_blk == ~0)
		return NULL;

	size = (info->super->num_blocks + 7) / 8;

	if (!(buf = malloc(size)))
		return NULL;

	fseeko(info->dev, bitmap_blk * info->super->blocksize, SEEK_SET);
	fread(buf, 1, size, info->dev);
	return buf;
}

int omfs_compute_hash(omfs_info_t *info, char *filename)
{
	int hash = 0, i;
	int m = (info->super->sys_blocksize - OMFS_DIR_START) / 8;
	
	for (i=0; i<strlen(filename); i++)
		hash ^= tolower(filename[i]) << (i % 24);
	
	return hash % m;
}

void omfs_sync(omfs_info_t *info)
{
	fflush(info->dev);
}
