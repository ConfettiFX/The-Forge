#pragma once
/* Compile with gcc -O3 -msse4.2 ... */
#ifdef _MSC_VER

#include <stdint.h>
#include <intrin.h>

// Byte-boundary alignment issues
#if defined(__x86_64__) || defined(_M_X64)
#define ALIGN_SIZE      0x08UL
#else
#define ALIGN_SIZE      0x04UL
#endif
#define ALIGN_MASK      (ALIGN_SIZE - 1)
#define CALC_CRC(op, crc, type, buf, len)                                      \
	{                                                                          \
		for (; (len) >= sizeof (type); (len) -= sizeof(type), buf += sizeof (type)) { \
			(crc) = (uint32_t)op((uint32_t)(crc), *(type *) (buf));                \
		}                                                                      \
	} 

static bool checkHashingHardwareSupport()
{
	int info[4];
	__cpuid(info, 0);

	int nids = info[0];
	if (nids < 1)
		return false;
	
	__cpuidex(info, 1, 0);
	return (info[2] & 0x100000) != 0; //check for SSE4.2 support
}

static uint32_t crc32c_hw(const void *input, int len, uint32_t crc)
{
	static bool hasHardwareSupport = checkHashingHardwareSupport();

	if (!hasHardwareSupport)
	{
		//Hashing algorithm from stb.h (stb_hash) by 'nothings' https://github.com/nothings/stb/blob/master/stb.h
		const uint8_t* buf = (const uint8_t*)input;
		static uint32_t crc_table[256];
		uint32_t len_u32 = (uint32_t)len;
		crc = ~crc;

		if (crc_table[1] == 0)
			for (uint32_t i = 0; i < 256; i++) {
				uint32_t s = i;
				for (uint32_t j = 0; j < 8; ++j)
					s = (s >> 1) ^ (s & 1 ? 0xedb88320 : 0);
				crc_table[i] = s;
			}
		for (uint32_t i = 0; i < len_u32; ++i)
			crc = (crc >> 8) ^ crc_table[buf[i] ^ (crc & 0xff)];
		return ~crc;

	}
	else
	{
		const char* buf = (const char*)input;
		crc ^= 0xFFFFFFFF;

		//In case the address we got is not aligned, we will hash single bytes,
		// until we reach a point where it is.
		for (; (len > 0) && ((size_t)buf & ALIGN_MASK); len--, buf++)
		{
			crc = (uint32_t)_mm_crc32_u8((uint32_t)crc, *(uint8_t*)buf);
		}

#if defined(__x86_64__) || defined(_M_X64)
		//We will now hash as many 64 bit chunks as possible
		CALC_CRC(_mm_crc32_u64, crc, uint64_t, buf, len);
#endif
		//Then we will hash as many 32 bit chunks as possible
		CALC_CRC(_mm_crc32_u32, crc, uint32_t, buf, len);
		//Then we will hash as many 16 bit chunks as possible
		CALC_CRC(_mm_crc32_u16, crc, uint16_t, buf, len);
		//Then we will hash as many 8 bit chunks as possible
		CALC_CRC(_mm_crc32_u8, crc, uint8_t, buf, len);

		//Flip the bits of the CRC code.
		return (crc ^ 0xFFFFFFFF);
	}
}


#endif