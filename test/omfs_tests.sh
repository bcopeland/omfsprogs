#! /bin/bash
#
# Various smoke tests for the OMFS filesystem driver
#
OMFSPROGS=..
BSIZE=4096

. assert.sh

# pass in: 
# filename(/device) block_size num_blocks
function make_disk
{
    if [[ ! -e $1 ]]; then
        dd if=/dev/zero of=$1 bs=$2 count=$3 2>/dev/null
    fi
    yes | $OMFSPROGS/mkomfs -x -b $2 $1 >/dev/null
}

function mount
{
    mkdir -p tmp
    sudo mount -t omfs -o loop,dmask=002,fmask=066,uid=$UID $1 tmp 2>/dev/null
}

function unmount
{
    sudo umount tmp
}

function begin
{
    echo -n "Testing $1........"
}

function get_avail
{
    AVAIL=`df -P -B $BSIZE tmp | tail -1 | awk '{print $4}'`
}

make_disk clean.img $BSIZE 5000

###############################################################
# bad magic
###############################################################
begin "magic number check"
cp clean.img bad_magic.img
dd if=/dev/zero of=bad_magic.img seek=272 bs=1 count=1 conv=notrunc 2>/dev/null
mount bad_magic.img
ret=$?
assert $ret 
rm bad_magic.img

###############################################################
# bad block size
###############################################################
begin "mount block size check"
make_disk bad_block.img 1024 5000
mount bad_block.img
ret=$?
assert $ret 
rm bad_block.img

###############################################################
# sysblock > PAGE_SIZE
###############################################################
begin "sysblock range check"
cp clean.img sysblock.img
# stuff 0x20 where it should be 0x10
echo -n  ' ' | dd of=sysblock.img seek=286 bs=1 count=1 conv=notrunc 2>/dev/null
mount sysblock.img
assert $?
rm sysblock.img

###############################################################
# block count discrepancy
###############################################################
begin "block count check"
cp clean.img nblock.img
dd if=/dev/zero of=nblock.img seek=270 bs=1 count=1 conv=notrunc 2>/dev/null
mount nblock.img
assert $?
rm nblock.img

###############################################################
# check create
###############################################################
begin "create"
cp clean.img create.img
mount create.img

# these are designed to collide...
touch tmp/10
dd if=/dev/urandom of=tmp/31 bs=$BSIZE count=10 2>/dev/null
touch tmp/53
touch tmp/75
echo "123456789" > tmp/11
touch tmp/30
touch tmp/53
touch tmp/72

assert_eq_s `stat -c %s tmp/10` 0
assert_eq_s `stat -c %s tmp/31` 40960
assert_eq_s `stat -c %s tmp/53` 0
assert_eq_s `stat -c %s tmp/75` 0
assert_eq_s `stat -c %s tmp/11` 10 
assert_eq_s `stat -c %s tmp/30` 0 
assert_eq_s `stat -c %s tmp/53` 0 
assert_eq_s `stat -c %s tmp/72` 0 

unmount create.img
$OMFSPROGS/omfsck -q create.img
assertz $?
rm create.img

###############################################################
# check rename
###############################################################
begin "rename"
cp clean.img rename.img
mount rename.img

mkdir -p tmp/foo
mkdir -p tmp/foo/bar

# overwrite descendent should fail
mv tmp/foo tmp/foo/bar 2>/dev/null
assert_s $?

touch tmp/foo/quux
dd if=/dev/urandom of=tmp/baz bs=$BSIZE count=10 2>/dev/null

# same file
mv tmp/foo/quux tmp/foo/quux 2>/dev/null
assert_s $?

# overwrite file w/ other file works
mv tmp/foo/quux tmp/baz
assertz_s $?

# same hash
mv tmp/baz tmp/bqr
assertz_s $?
# different hash
mv tmp/bqr tmp/bqa
assertz_s $?
unmount
$OMFSPROGS/omfsck -q rename.img
assertz $?
rm rename.img

###############################################################
# create/rm checking bitmap
###############################################################
begin "bitmap"
cp clean.img bitmap.img
mount bitmap.img
get_avail
assert_eq_s $AVAIL 4994

dd if=/dev/urandom of=tmp/myfile2.bin bs=600 count=1 2>/dev/null
dd if=/dev/urandom of=tmp/myfile.bin bs=$BSIZE count=10 2>/dev/null

# 2 per inode + (8 + blocks-8)
get_avail
assert_eq_s $AVAIL 4972

rm tmp/myfile2.bin
get_avail
assert_eq_s $AVAIL 4982

rm tmp/myfile.bin
get_avail
assert_eq_s $AVAIL 4994
unmount
$OMFSPROGS/omfsck -q bitmap.img
ret=$?
assertz $ret
rm bitmap.img

rm clean.img
