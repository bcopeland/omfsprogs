/* Routines for bitmap allocation */
#include <stdlib.h>
#include "omfs.h"
#include "bits.h"
#include "errno.h"


static int nibblemap[] = {4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

unsigned long omfs_count_free(omfs_info_t *info)
{
    unsigned int i;
    unsigned long sum = 0;
    size_t bsize = (swap_be64(info->super->s_num_blocks) + 7) / 8;

    u8 *map = info->bitmap->bmap;

    for (i = 0; i < bsize; i++)
        sum += nibblemap[map[i] & 0xf] + nibblemap[(map[i] >> 4) & 0xf];

    return sum;
}

/*
 *  Mark the bitmap block which holds an allocated/cleared bit dirty.
 *  Call after every set_bit/clear_bit in the bitmap.  Block is the
 *  *data* block number.
 */
static void mark_dirty(omfs_info_t *info, u64 block)
{
    int blocksize = swap_be32(info->super->s_blocksize);
    u64 bit_blk = (block >> 3) / blocksize;
    set_bit(info->bitmap->dirty, bit_blk);
}

/* 
 * Scan through a bitmap for power-of-two sized region (max 8).  This 
 * should help to keep down fragmentation as mirrors will generally 
 * align to 2 blocks and clusters to 8.
 */
static int scan(u8* buf, int bsize, int bits)
{
    int shift = 1 << bits;
    int mask = shift - 1;
    int m, i;

    for (i=0; i < bsize * 8; i += bits)
    {
        m = mask << (i & 7);
        if (!(buf[ i >> 3 ] & m))
        {
            buf[ i >> 3 ] |= m;
            break;
        }
    }
    return i;
}

int omfs_clear_range(omfs_info_t *info, u64 start, int count)
{
    int i;
    u8 *bitmap = info->bitmap->bmap;

    for (i=0; i < count; i++) {
        clear_bit(bitmap, i + start);
        mark_dirty(info, i + start);
    }

    omfs_flush_bitmap(info);
    return 0;
}

int omfs_allocate_one_block(omfs_info_t *info, u64 block)
{
    int ok = 0;
    u8 *bitmap = info->bitmap->bmap;

    if (!test_bit(bitmap, block))
    {
        set_bit(bitmap, block);
        mark_dirty(info, block);
        omfs_flush_bitmap(info);
        ok = 1;
    }
    return ok;
}

int omfs_allocate_block(omfs_info_t *info, int size, u64 *return_block)
{
    size_t bsize;
    int ret = 0;
    int block, i;

    u8 *bitmap = info->bitmap->bmap;

    bsize = (swap_be64(info->super->s_num_blocks) + 7) / 8;

    block = scan(bitmap, bsize, size);
    if (block == bsize * 8) {
        ret = -ENOSPC;
        goto out;
    }
    *return_block = block;

    for (i=0; i < size; i++)
        mark_dirty(info, block + i);
    
    omfs_flush_bitmap(info);
out:
    return ret;
}

