/*
 * disksize.c - Try to get the size of a disk.  
 *
 * At present this relies on fseeko and such.  Could do something 
 * smarter here like a binary search if this needs to be more portable.
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include "disksize.h"

int get_disk_size(char *device, u64 *size)
{
	FILE *fp = fopen(device, "r");
	if (!fp)
		return 0;

	fseeko(fp, 0, SEEK_END);
	*size = ftello(fp);
	return 1;
}
