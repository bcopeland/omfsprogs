/*
 *  Super/root block reading routines
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "omfs.h"
#include "bits.h"
#include "crc.h"

static void _omfs_make_empty_table(u8 *buf, int offset)
{
    struct omfs_extent *oe = (struct omfs_extent *) &buf[offset];
    oe->e_next = ~0ULL;
    oe->e_extent_count = swap_be32(1),
    oe->e_fill = swap_be32(0x22),
    oe->e_entry.e_cluster = ~0ULL;
    oe->e_entry.e_blocks = ~0ULL;
}


static void _omfs_swap_buffer(void *buf, int count)
{
    int i;
    u32 *ibuf = (u32 *) buf;

    count >>= 2;

    for (i=0; i<count; i++)
        ibuf[i] = __swap32(ibuf[i]);
}

/*
 * Write the superblock to disk
 */
int omfs_write_super(omfs_info_t *info)
{
    int count;

    if (info->swap)
        _omfs_swap_buffer(info->super, sizeof(struct omfs_super_block));

    pthread_mutex_lock(&info->dev_mutex);
    fseeko(info->dev, 0LL, SEEK_SET);
    count = fwrite(info->super, 1, sizeof(struct omfs_super_block), info->dev);
    pthread_mutex_unlock(&info->dev_mutex);

    if (info->swap)
        _omfs_swap_buffer(info->super, sizeof(struct omfs_super_block));

    if (count < sizeof(struct omfs_super_block))
        return -1;

    return 0;
}

/*
 * Read the superblock and store the result in ret
 */
int omfs_read_super(omfs_info_t *info)
{
    int count, err = 0;

    pthread_mutex_lock(&info->dev_mutex);
    fseeko(info->dev, 0LL, SEEK_SET);
    count = fread(info->super, 1, sizeof(struct omfs_super_block), info->dev);
    pthread_mutex_unlock(&info->dev_mutex);

    if (count < sizeof(struct omfs_super_block)) 
    {
        err = -EIO;
        goto out;
    }

    info->swap = 0;
    if (info->super->s_magic == OMFS_MAGIC)       // unswapped
    {
        _omfs_swap_buffer(info->super, count);
        info->swap = 1;
    }
    else if (swap_be32(info->super->s_magic) != OMFS_MAGIC)
        err = -EMEDIUMTYPE;

out:
    return err;
}

static int _omfs_write_block(omfs_info_t *info, 
        u64 block, u8* buf, size_t len, int mirrors)
{
    int i, count, ret = 0;
    FILE *dev = info->dev;
    struct omfs_super_block *sb = info->super;

    if (info->swap)
        _omfs_swap_buffer(buf, len);

    pthread_mutex_lock(&info->dev_mutex);
    for (i=0; i<mirrors; i++)
    {
        fseeko(dev, (block + i) * swap_be32(sb->s_blocksize), SEEK_SET);
        count = fwrite(buf, 1, len, dev);
        if (count != len)
        {
            ret = -1;
            goto out;
        }
    }
out:
    if (info->swap)
        _omfs_swap_buffer(buf, len);
    pthread_mutex_unlock(&info->dev_mutex);
    return ret;
}


/*
 * Read the numbered block into a raw array.
 * buf must be at least blocksize bytes.
 */
static int _omfs_read_block(omfs_info_t *info, u64 block, u8 *buf)
{
    int count, ret = 0;
    FILE *dev = info->dev;
    struct omfs_super_block *sb = info->super;
    int blocksize;

    blocksize = swap_be32(sb->s_blocksize);

    pthread_mutex_lock(&info->dev_mutex);
    fseeko(dev, block * blocksize, SEEK_SET);
    count = fread(buf, 1, blocksize, dev);
    pthread_mutex_unlock(&info->dev_mutex);

    if (info->swap)
        _omfs_swap_buffer(buf, count);

    if (count < blocksize)
        ret = -1;

    return ret;
}

static void _update_header_checksums(u8 *buf, int block_size) 
{
    int xor, i;
    omfs_header_t *header = (omfs_header_t *) buf;
    u8 *body = buf + sizeof(omfs_header_t);
    int body_size = block_size - sizeof(omfs_header_t);

    header->h_crc = swap_be16(crc_ccitt_msb(0, body, body_size));

    xor = buf[0];
    for (i=1; i<OMFS_XOR_COUNT; i++)
        xor ^= buf[i];
    header->h_check_xor = xor;
}


int omfs_write_root_block(omfs_info_t *info)
{
    u64 block = swap_be64(info->root->r_head.h_self);
    return _omfs_write_block(info, block, (u8*) info->root, 
            sizeof(struct omfs_root_block), 
            swap_be32(info->super->s_mirrors));
}


static u8 *_omfs_get_block(omfs_info_t *info, u64 block)
{
    u8 *buf;
    if (!(buf = malloc(swap_be32(info->super->s_blocksize))))
        return 0;

    if (_omfs_read_block(info, block, buf))
    {
        free(buf);
        return 0;
    }

    return buf;
}

u8 *omfs_get_block(omfs_info_t *info, u64 block)
{
    return _omfs_get_block(info, block);
}

int omfs_read_root_block(omfs_info_t *info)
{
    u8 *buf;

    buf = _omfs_get_block(info, swap_be64(info->super->s_root_block));
    if (!buf)
        return -1;

    memcpy(info->root, buf, sizeof(struct omfs_root_block));
    free(buf);
    return 0;
}

/*
 *  Write an inode to the device.
 */
int omfs_write_inode(omfs_info_t *info, omfs_inode_t *inode)
{
    struct timezone tz;
    struct timeval tv;
    u64 ctime;
    int size = swap_be32(inode->i_head.h_body_size) + sizeof(omfs_header_t);

    gettimeofday(&tv, &tz);

    ctime = tv.tv_sec * 1000LL + tv.tv_usec;
    inode->i_ctime = swap_be64(ctime);

    _update_header_checksums((u8*)inode, size);

    return _omfs_write_block(info, swap_be64(inode->i_head.h_self), 
            (u8*) inode, size, swap_be32(info->super->s_mirrors));
}

omfs_inode_t *omfs_new_inode(omfs_info_t *info, u64 block, 
        char *name, char type)
{
    u8 *buf;
    omfs_inode_t *inode;

    inode = omfs_get_inode(info, block);
    if (!inode)
        return NULL;

    memset(inode, 0, swap_be32(info->super->s_blocksize));

    inode->i_head.h_self = swap_be64(block); 
    inode->i_head.h_version = 1;
    inode->i_head.h_magic = OMFS_IMAGIC;
    inode->i_head.h_body_size = swap_be32(
        swap_be32(info->super->s_sys_blocksize) - sizeof(struct omfs_header));
    inode->i_head.h_type = OMFS_INODE_NORMAL;
    inode->i_type = type;
    inode->i_parent = ~0ULL;
    inode->i_sibling = ~0ULL;
    inode->i_fill2 = swap_be32(1);
    strncpy(inode->i_name, name, OMFS_NAMELEN);
    inode->i_name[OMFS_NAMELEN-1] = 0;
    inode->i_size = swap_be64((u64) swap_be32(info->super->s_sys_blocksize));

    buf = (u8*) inode;
    if (type == OMFS_FILE)
    {
        inode->i_size = 0;
        _omfs_make_empty_table(buf, OMFS_EXTENT_START);
    }
    else if (type == OMFS_INODE_CONTINUATION)
    {
        inode->i_head.h_type = OMFS_INODE_CONTINUATION;
        _omfs_make_empty_table(buf, OMFS_EXTENT_CONT);
    }
    else
    {
        memset(&buf[OMFS_DIR_START], 0xff, 
            swap_be32(info->super->s_sys_blocksize) - OMFS_DIR_START);
    }

    return inode;
}

omfs_inode_t *omfs_get_inode(omfs_info_t *info, u64 block)
{
    u8 *buf;
    buf = omfs_get_block(info, block);
    if (!buf)
        return NULL;

    return (omfs_inode_t *) buf;
}

void omfs_release_inode(omfs_inode_t *oi)
{
    free(oi);
}

int omfs_flush_bitmap(omfs_info_t *info)
{
    size_t size, bsize, count, ret = 0;
    u64 bitmap_blk = swap_be64(info->root->r_bitmap);
    int blocksize = swap_be32(info->super->s_blocksize);
    u8 *bmap = info->bitmap->bmap;
    int i;

    if (bitmap_blk == ~0)
        return 0;

    size = (swap_be64(info->super->s_num_blocks) + 7) / 8;
    bsize = (size + blocksize - 1) / blocksize;

    pthread_mutex_lock(&info->dev_mutex);
    for (i=0; i < bsize * 8; i++, bitmap_blk++, bmap += blocksize)
    {
        if (test_bit(info->bitmap->dirty, i)) 
        {
            fseeko(info->dev, bitmap_blk * blocksize, SEEK_SET);
            count = fwrite(bmap, 1, blocksize, info->dev);
            if (size != count) {
                ret = -EIO;
                goto out;
            }
            clear_bit(info->bitmap->dirty, i);
        }
    }

out:
    pthread_mutex_unlock(&info->dev_mutex);
    return ret;
}

int omfs_write_block(omfs_info_t *info, u64 block, u8* buf)
{
    return _omfs_write_block(info, block, buf, 
        swap_be32(info->super->s_blocksize), 1);
}

static void set_inuse_file(omfs_info_t *info, omfs_inode_t *file, u8 *bmap)
{
    struct omfs_extent *oe;
    struct omfs_extent_entry *entry;
    u64 next;
    int extent_count, i;
    omfs_inode_t *inode;

    if (!file)
        return;

    next = swap_be64(file->i_head.h_self);
    inode = file;
    oe = (struct omfs_extent *) (((u8*) inode) + OMFS_EXTENT_START);

    for(;;) 
    {
        extent_count = swap_be32(oe->e_extent_count);

        for (i=0; i<swap_be32(info->super->s_mirrors); i++)
            set_bit(bmap, next + i);

        next = swap_be64(oe->e_next);
        entry = &oe->e_entry;

        // ignore last entry as it is the terminator
        for (; extent_count > 1; extent_count--)
        {
            u64 start = swap_be64(entry->e_cluster);

            for (i=0; i<swap_be64(entry->e_blocks); i++)
                set_bit(bmap, start + i);

            entry++;
        }

        if (next == ~0)
            break;

        if (inode != file)
            omfs_release_inode(inode);

        inode = omfs_get_inode(info, next);
        if (!inode)
            goto err;

        oe = (struct omfs_extent *) (((u8*) inode) + OMFS_EXTENT_CONT);
    }

    if (inode != file)
        omfs_release_inode(inode);
err:
    return;
}

static void set_inuse_dir(omfs_info_t *info, omfs_inode_t *dir, u8 *bmap)
{
    u64 *ptr;
    int i;

    if (!dir)
        return;
        
    int num_entries = (swap_be32(dir->i_head.h_body_size) + 
        sizeof(omfs_header_t) - OMFS_DIR_START) / 8;

    ptr = (u64*) ((u8*) dir + OMFS_DIR_START);

    for (i=0; i<swap_be32(info->super->s_mirrors); i++)
        set_bit(bmap, swap_be64(dir->i_head.h_self) + i);

    for (i=0; i<num_entries; i++, ptr++)
    {
        u64 inum = swap_be64(*ptr);
        while (inum != ~0)
        {
            omfs_inode_t *tmp = omfs_get_inode(info, inum);
            if (!tmp)
                continue;

            if (tmp->i_type == OMFS_DIR)
                set_inuse_dir(info, tmp, bmap);
            else
                set_inuse_file(info, tmp, bmap);

            inum = swap_be64(tmp->i_sibling);
            omfs_release_inode(tmp);
        }
    }
}

static void set_inuse_bits(omfs_info_t *info)
{
    u8 *bmap = info->bitmap->bmap;  
    u64 root_dir;
    int i;
    omfs_inode_t *root;

    root_dir = swap_be64(info->root->r_root_dir);
    
    for (i=0; i<root_dir; i++)
        set_bit(bmap, i);
    
    root = omfs_get_inode(info, root_dir);
    set_inuse_dir(info, root, bmap);
}

int omfs_load_bitmap(omfs_info_t *info)
{
    size_t size, dirty_size;
    u8 *buf;
    u8 *dirty_bits;
    u64 bitmap_blk = swap_be64(info->root->r_bitmap);
    int blocksize = swap_be32(info->super->s_blocksize);
    int size_blks;
    struct omfs_bitmap *bitmap;
    int ret = 0;

    size = (swap_be64(info->super->s_num_blocks) + 7) / 8;

    // round up to a full block
    size_blks = (size + blocksize - 1) / blocksize;
    size = size_blks * blocksize;

    dirty_size = (size_blks + 7) / 8;

    if (!(buf = malloc(size))) {
        ret = -ENOMEM;
        goto out1;
    }

    if (!(dirty_bits = calloc(1, dirty_size))) {
        ret = -ENOMEM;
        goto out2;
    }

    bitmap = malloc(sizeof(struct omfs_bitmap));
    if (!bitmap) {
        ret = -ENOMEM;
        goto out3;
    }

    info->bitmap = bitmap; 
    bitmap->dirty = dirty_bits;
    bitmap->bmap = buf;

    if (bitmap_blk == ~0)
    {
        // create the bitmap by traversal
        memset(buf, 0, size);
        set_inuse_bits(info);
    }
    else
    {
        pthread_mutex_lock(&info->dev_mutex);
        fseeko(info->dev, bitmap_blk * blocksize, SEEK_SET);
        fread(buf, 1, size, info->dev);
        pthread_mutex_unlock(&info->dev_mutex);
    }
    goto out1;

out3:
    free(dirty_bits);
out2:
    free(buf);
out1:
    return ret;
}

int omfs_compute_hash(omfs_info_t *info, char *filename)
{
    int hash = 0, i;
    int m = (swap_be32(info->super->s_sys_blocksize) - OMFS_DIR_START) / 8;
    
    for (i=0; i<strlen(filename); i++)
        hash ^= tolower(filename[i]) << (i % 24);
    
    return hash % m;
}

void omfs_sync(omfs_info_t *info)
{
    fflush(info->dev);
}

void omfs_clear_data(omfs_info_t *info, u64 block, int count)
{
    int i;

    for (i=0; i < count; i++, block++)
    {
        u8 *buf = omfs_get_block(info, block);
        if (!buf)
            return;

        memset(buf, 0, swap_be32(info->super->s_blocksize));
        omfs_write_block(info, block, buf);
        free(buf);
    }
}

