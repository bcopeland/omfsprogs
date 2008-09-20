/*
 *  Filesystem check for OMFS
 */
#include <stdlib.h>
#include "dump.h"

int main(int argc, char *argv[])
{
	FILE *fp;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <device>\n", argv[0]);
		exit(1);
	}

	fp = fopen(argv[1], "rb");
	if (!fp)
	{
		perror("omfsdump: ");
		exit(2);
	}

    dump_fs(fp);
    return 0;
}
