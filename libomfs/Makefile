LIBOMFS_SRCS=crc.c omfs.c bitmap.c
LIBOMFS_OBJS=$(LIBOMFS_SRCS:.c=.o)

CFLAGS=-g -Wall -Wpadded -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

all: libomfs.a

libomfs.a: libomfs.a($(LIBOMFS_OBJS))

clean:
	$(RM) libomfs.a *.o
