COMMON_SRCS=../crc.c ../omfs.c 
COMMON_OBJS=$(COMMON_SRCS:.c=.o)
BINS=bad_hash bad_crc bad_bitmap loop

CFLAGS=-g -Wall -Wpadded -I..

all: $(BINS)

bad_hash: $(COMMON_OBJS) bad_hash.o
	gcc -o bad_hash $(COMMON_OBJS) bad_hash.o

bad_crc: $(COMMON_OBJS) bad_crc.o
	gcc -o bad_crc $(COMMON_OBJS) bad_crc.o

bad_bitmap: $(COMMON_OBJS) bad_bitmap.o
	gcc -o bad_bitmap $(COMMON_OBJS) bad_bitmap.o

loop: $(COMMON_OBJS) loop.o
	gcc -o loop $(COMMON_OBJS) loop.o

clean:
	$(RM) $(BINS) *.o *.img

images: all
	dd if=/dev/zero of=base.img count=100 bs=2048
	yes | ../mkomfs -b 2048 base.img > /dev/null
	cp base.img bad_hash.img && ./bad_hash bad_hash.img
	cp base.img bad_crc.img && ./bad_crc bad_crc.img
	cp base.img bad_bitmap.img && ./bad_bitmap bad_bitmap.img
	cp base.img loop.img && ./loop loop.img
