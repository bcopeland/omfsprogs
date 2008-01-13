#ifndef CREATE_FS_H
#define CREATE_FS_H

#include "config.h"

typedef struct _fs_config
{
	int block_size;
	int cluster_size;
	int clear_dev;
} fs_config_t;

int create_fs(FILE *fp, u64 dev_blks, fs_config_t *config);

#endif
