/*
 *  Filesystem check for OMFS
 */
#include <stdlib.h>
#include <getopt.h>
#include "config.h"
#include "omfs.h"
#include "dirscan.h"
#include "check.h"

int main(int argc, char *argv[])
{
	FILE *fp;
	char *dev;

	check_fs_config_t config = {
		.is_quiet = 0,
	};

	while (1) 
	{
		int c;

		c = getopt(argc, argv, "q");
		if (c == -1)
			break;

		switch(c)
		{
			case 'q':
				config.is_quiet = 1;
				break;
		}
	}

	if (argc - optind < 1)
	{
		fprintf(stderr, "Usage: %s [options] <device>\n", argv[0]);
		exit(1);
	}

	dev = argv[optind];

	fp = fopen(dev, "r+");
	if (!fp)
	{
		perror("omfsck: ");
		exit(2);
	}

	if (!check_fs(fp, &config))
	{
		fclose(fp);
		exit(3);
	}
	fclose(fp);

	if (!config.is_quiet)
		printf("File system check successful\n");
	return 0;
}
