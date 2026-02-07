/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_SCRIPTS_BYTESWAP_H
#define LINUX_SCRIPTS_BYTESWAP_H

/*
 * Some host build environments (notably macOS) do not ship <byteswap.h>.
 * Kbuild host tools (eg. modpost) include it for bswap_{16,32,64}().
 */

#if !defined(__APPLE__)
#include_next <byteswap.h>
#else
#include <stdint.h>

static inline uint16_t __bswap_16(uint16_t x)
{
	return __builtin_bswap16(x);
}

static inline uint32_t __bswap_32(uint32_t x)
{
	return __builtin_bswap32(x);
}

static inline uint64_t __bswap_64(uint64_t x)
{
	return __builtin_bswap64(x);
}

#define bswap_16(x) __bswap_16((uint16_t)(x))
#define bswap_32(x) __bswap_32((uint32_t)(x))
#define bswap_64(x) __bswap_64((uint64_t)(x))

#endif /* __APPLE__ */

#endif /* LINUX_SCRIPTS_BYTESWAP_H */
