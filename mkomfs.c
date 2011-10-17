/*  mkomfs.c - Create an OMFS filesystem
 *  
 *  Licensed under GPL version 2 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#include "config.h"
#include "disksize.h"
#include "create_fs.h"

#include <getopt.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	char *dev;
	u64 size;

	fs_config_t config = {
		.block_size = 8192,
		.cluster_size = 8,
		.clear_dev = 0
	};

	while (1) 
	{
		int c;

		c = getopt(argc, argv, "b:c:x");
		if (c == -1)
			break;

		switch(c)
		{
			case 'b':
				config.block_size = atoi(optarg);
				break;
			case 'c':
				config.cluster_size = atoi(optarg);
				break;
			case 'x':
				config.clear_dev = 1;
				break;
		}
	}

	if (argc - optind < 1)
	{
		fprintf(stderr, "Usage: %s [options] <device>\n", argv[0]);
		exit(1);
	}

	dev = argv[optind];

	if (!get_disk_size(dev, &size))
	{
		fprintf(stderr, "Could not get size of disk %s\n", 
				argv[optind]);
		exit(1);
	}

	printf("Creating a new fs on dev %s (%" PRIu64 " blks)\n", dev, size/512);

	printf("Warning: this could kill some important data; Are you sure? ");
	char ch = getchar();

	if (ch != 'y')
		exit(0);

	fp = fopen(dev, "r+");
	if (!fp)
	{
		perror("mkomfs: ");
		exit(2);
	}

	create_fs(fp, size/512, &config);
	fclose(fp);
	return 0;
}
