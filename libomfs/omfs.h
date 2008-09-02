#ifndef _OMFS_H
#define _OMFS_H

#include <stdio.h>
#include "config.h"
#include "omfs_fs.h"
#include "crc.h"

struct omfs_info {
    FILE *dev;
    struct omfs_super_block *super;
    struct omfs_root_block *root;
    struct omfs_bitmap *bitmap;
    int swap;
    pthread_mutex_t dev_mutex;
};

struct omfs_bitmap {
    u8 *dirty;
    u8 *bmap;
};

typedef struct omfs_info omfs_info_t;
typedef struct omfs_header omfs_header_t;
typedef struct omfs_super_block omfs_super_t;
typedef struct omfs_bitmap omfs_bitmap_t;
typedef struct omfs_root_block omfs_root_t;
typedef struct omfs_inode omfs_inode_t;

int omfs_read_super(omfs_info_t *info);
int omfs_write_super(omfs_info_t *info);
int omfs_read_root_block(omfs_info_t *info);
int omfs_write_root_block(omfs_info_t *info);
u8 *omfs_get_block(omfs_info_t *info, u64 block);
int omfs_write_block(omfs_info_t *info, u64 block, u8* buf);
void omfs_release_block(u8 *buf);
int omfs_check_crc(u8 *blk);
omfs_inode_t *omfs_get_inode(omfs_info_t *info, u64 block);
int omfs_write_inode(omfs_info_t *info, omfs_inode_t *inode);
void omfs_release_inode(omfs_inode_t *inode);
void omfs_sync(omfs_info_t *info);
int omfs_load_bitmap(omfs_info_t *info);
int omfs_flush_bitmap(omfs_info_t *info);
int omfs_compute_hash(omfs_info_t *info, char *filename);
omfs_inode_t *omfs_new_inode(omfs_info_t *info, u64 block, char *name, 
    char type);
void omfs_clear_data(omfs_info_t *info, u64 block, int count);

/* bitmap.c */
int omfs_allocate_one_block(omfs_info_t *info, u64 block);
int omfs_allocate_block(omfs_info_t *info, int size, u64 *return_block);
int omfs_clear_range(omfs_info_t *info, u64 start, int count);
unsigned long omfs_count_free(omfs_info_t *info);

#endif
