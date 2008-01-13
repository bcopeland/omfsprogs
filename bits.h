#ifndef _BITS_H
#define _BITS_H

inline int test_bit(u8* buf, u64 offset)
{
	return buf[offset >> 3] & (1<<(offset & 7));
}

inline void init_bit(u8* buf, u64 offset, int value)
{
	int mask = ((value & 1) << (offset & 7));
	buf[offset >> 3] &= ~mask;
	buf[offset >> 3] |= mask;
}
	
inline void clear_bit(u8* buf, u64 offset)
{
	buf[offset >> 3] &= ~(1 << (offset & 7));
}

inline void set_bit(u8* buf, u64 offset)
{
	buf[offset >> 3] |= 1 << (offset & 7);
}

#endif
