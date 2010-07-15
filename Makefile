VERSION=0.1.0
DISTNAME=omfsprogs-$(VERSION)
DISTFILES=*.[ch] Makefile README COPYING
LIBFILES=libomfs/*.[ch] libomfs/Makefile
TESTFILES=test/*.[ch] test/Makefile test/*.sh

COMMON_SRCS=dirscan.c stack.c io.c
COMMON_OBJS=$(COMMON_SRCS:.c=.o)

OMFSCK_SRCS=omfsck.c fix.c check.c
OMFSCK_OBJS=$(OMFSCK_SRCS:.c=.o) $(COMMON_OBJS)

MKOMFS_SRCS=mkomfs.c create_fs.c disksize.c
MKOMFS_OBJS=$(MKOMFS_SRCS:.c=.o) $(COMMON_OBJS)

OMFSDUMP_SRCS=omfsdump.c dump.c
OMFSDUMP_OBJS=$(OMFSDUMP_SRCS:.c=.o) $(COMMON_OBJS)

CFLAGS=-g -Wall -Wpadded -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I libomfs
LIBS=-Llibomfs -lomfs

all: omfsck mkomfs omfsdump

libomfs: .PHONY
	cd libomfs && $(MAKE)

omfsck: $(OMFSCK_OBJS) libomfs
	gcc -o omfsck $(OMFSCK_OBJS) $(LIBS)

mkomfs: $(MKOMFS_OBJS) libomfs
	gcc -o mkomfs $(MKOMFS_OBJS) $(LIBS)

omfsdump: $(OMFSDUMP_OBJS) libomfs
	gcc -o omfsdump $(OMFSDUMP_OBJS) $(LIBS)

clean:
	$(RM) omfsck mkomfs *.o
	cd libomfs && $(MAKE) clean
	cd test && $(MAKE) clean

dist: clean
	mkdir $(DISTNAME)
	mkdir $(DISTNAME)/libomfs
	mkdir $(DISTNAME)/test
	cp $(DISTFILES) $(DISTNAME)
	cp $(LIBFILES) $(DISTNAME)/libomfs
	cp $(TESTFILES) $(DISTNAME)/test
	tar czvf $(DISTNAME).tar.gz $(DISTNAME)
	$(RM) -r $(DISTNAME)

distcheck: dist
	mkdir build
	cd build && tar xzvf ../$(DISTNAME).tar.gz && \
	cd $(DISTNAME) && $(MAKE) && \
	cd test && $(MAKE)
	$(RM) -r build

.PHONY:
