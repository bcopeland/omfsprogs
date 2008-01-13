/*
 *  Filesystem check for OMFS
 */
#include <stdlib.h>
#include "config.h"
#include "omfs.h"
#include "dirscan.h"
#include "check.h"

int main(int argc, char *argv[])
{
	FILE *fp;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <device>\n", argv[0]);
		exit(1);
	}

	fp = fopen(argv[1], "r+");
	if (!fp)
	{
		perror("omfsck: ");
		exit(2);
	}

	if (!check_fs(fp))
	{
		fclose(fp);
		exit(3);
	}
	fclose(fp);
	printf("File system check successful\n");
	return 0;
}
