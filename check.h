#ifndef _CHECK_H
#define _CHECK_H

#include "config.h"
#include "omfs.h"

typedef struct _check_fs_config
{
	int is_quiet;
} check_fs_config_t;

typedef enum 
{
	E_NONE,
	E_HEADER_XOR,
	E_HEADER_CRC,
	E_BIT_SET,
	E_BIT_CLEAR,
	E_BITMAP,
	E_HASH_WRONG,
	E_BLOCKSIZE,
	E_SYS_BLOCKSIZE,
	E_MIRRORS,
	E_EXTENT_COUNT,
	E_TERMINATOR,
	E_MAGIC,
	E_FILE_MAGIC,
	E_SELF_PTR,
	E_PARENT_PTR,
	E_READ_SUPER,
	E_READ_ROOT,
	E_INSANE,
	E_SCAN,
	E_LOOP
} check_error_t;

typedef struct check_context
{
	check_fs_config_t *config;	
	u8 *bitmap;
	u8 *visited;
	omfs_inode_t *current_inode;
	omfs_info_t *omfs_info;
	u64 parent;                /* parent inode number */
	u64 block;
	int hash;
} check_context_t;

int check_fs(FILE *fp, check_fs_config_t *config);
int check_fix(check_context_t *fix, check_error_t error_code);

#endif
