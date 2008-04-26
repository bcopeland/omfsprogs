#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "omfs.h"
#include "bits.h"
#include "config.h"

void make_bad_crc(FILE *fp)
{
	char crc[] = {0, 0};
	omfs_super_t super;
	omfs_root_t root;

	omfs_read_super(fp, &super);
	omfs_read_root_block(fp, &super, &root);

	fseeko(fp, swap_be64(root.root_dir) * swap_be32(super.blocksize) +
		12, SEEK_SET);
	fwrite(crc, 2, 1, fp);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 1;

	FILE *fp = fopen(argv[1], "r+");

	make_bad_crc(fp);
	fclose(fp);
	return 0;
}

