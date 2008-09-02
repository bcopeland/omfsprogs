#ifndef _CONFIG_H
#define _CONFIG_H

#include <inttypes.h>

typedef unsigned char u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef char s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u64 __be64;
typedef u32 __be32;
typedef u16 __be16;

#if __BYTE_ORDER == __BIG_ENDIAN
#define swap_be64(a) (a)
#define swap_be32(a) (a)
#define swap_be16(a) (a)
#else
#define swap_be64(a) __swap64(a)
#define swap_be32(a) __swap32(a)
#define swap_be16(a) __swap16(a)
#endif

static inline u16 __swap16(u16 a)
{
	return (((a & 0xff00) >> 8) | 
	        ((a & 0x00ff) << 8));
}

static inline u32 __swap32(u32 a)
{
	return (((a & 0xff000000U) >> 24) | 
	        ((a & 0x00ff0000U) >> 8) | 
	        ((a & 0x0000ff00U) << 8) | 
	        ((a & 0x000000ffU) << 24));
}

static inline u64 __swap64(u64 a)
{
	return (((a & 0xff00000000000000ULL) >> 56) |
		((a & 0x00ff000000000000ULL) >> 40) |
		((a & 0x0000ff0000000000ULL) >> 24) |
		((a & 0x000000ff00000000ULL) >> 8)  |
		((a & 0x00000000ff000000ULL) << 8)  |
		((a & 0x0000000000ff0000ULL) << 24) |
		((a & 0x000000000000ff00ULL) << 40) |
		((a & 0x00000000000000ffULL) << 56));
}


#endif /* _CONFIG_H */
