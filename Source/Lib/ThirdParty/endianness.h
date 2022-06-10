/** 
* @file   endianness.h
* @brief  Convert Endianness of shorts, longs, long longs, regardless of architecture/OS
*
* Defines (without pulling in platform-specific network include headers):
* bswap16, bswap32, bswap64, ntoh16, hton16, ntoh32 hton32, ntoh64, hton64
*
* Should support linux / macos / solaris / windows.
* Supports GCC (on any platform, including embedded), MSVC2015, and clang,
* and should support intel, solaris, and ibm compilers as well.
*
* Copyright 2020 github user jtbr, Released under MIT license
* Updated by MediaArea.net with l and b swap variants + C++ overloads
*/

#ifndef ENDIANNESS_H_
#define ENDIANNESS_H_

#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
#include <cstring> // for memcpy
#endif

/* Detect platform endianness at compile time */

// If boost were available on all platforms, could use this instead to detect endianness
// #include <boost/predef/endian.h>

// When available, these headers can improve platform endianness detection
#ifdef __has_include // C++17, supported as extension to C++11 in clang, GCC 5+, vs2015
#  if __has_include(<endian.h>)
#    include <endian.h> // gnu libc normally provides, linux
#  elif __has_include(<machine/endian.h>)
#    include <machine/endian.h> //open bsd, macos
#  elif __has_include(<sys/param.h>)
#    include <sys/param.h> // mingw, some bsd (not open/macos)
#  elif __has_include(<sys/isadefs.h>)
#    include <sys/isadefs.h> // solaris
#  endif
#endif

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#  if (defined(__BYTE_ORDER__)  && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || \
     (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) || \
	 (defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN) || \
	 (defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN) || \
     (defined(__sun) && defined(__SVR4) && defined(_BIG_ENDIAN)) || \
     defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
     defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__) || \
     defined(_M_PPC)
#    define __BIG_ENDIAN__
#  elif (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || /* gcc */\
     (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) /* linux header */ || \
	 (defined(_BYTE_ORDER) && _BYTE_ORDER == _LITTLE_ENDIAN) || \
	 (defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN) /* mingw header */ ||  \
     (defined(__sun) && defined(__SVR4) && defined(_LITTLE_ENDIAN)) || /* solaris */ \
     defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
     defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
     defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64) || /* msvc for intel processors */ \
     defined(_M_ARM) /* msvc code on arm executes in little endian mode */
#    define __LITTLE_ENDIAN__
#  endif
#endif

#if !defined(__LITTLE_ENDIAN__) & !defined(__BIG_ENDIAN__)
#  error "UNKNOWN Platform / endianness. Configure endianness checks for this platform or set explicitly."
#endif

#if defined(bswap16) || defined(bswap32) || defined(bswap64) || defined(bswapf) || defined(bswapd)
#  error "unexpected define!" // freebsd may define these; probably just need to undefine them
#endif

/* Define byte-swap functions, using fast processor-native built-ins where possible */
#if defined(_MSC_VER) // needs to be first because msvc doesn't short-circuit after failing defined(__has_builtin)
#  define bswap16(x)     _byteswap_ushort((x))
#  define bswap32(x)     _byteswap_ulong((x))
#  define bswap64(x)     _byteswap_uint64((x))
#elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#  define bswap16(x)     __builtin_bswap16((x))
#  define bswap32(x)     __builtin_bswap32((x))
#  define bswap64(x)     __builtin_bswap64((x))
#elif defined(__has_builtin)
#  if __has_builtin (__builtin_bswap64)  /* for clang; gcc 5 fails on this and && shortcircuit fails; must be after GCC check */
#    define bswap16(x)     __builtin_bswap16((x))
#    define bswap32(x)     __builtin_bswap32((x))
#    define bswap64(x)     __builtin_bswap64((x))
#  endif
#endif

    /* even in this case, compilers often optimize by using native instructions */
#if !defined(bswap16)
    static inline uint16_t bswap16(uint16_t x) {
        return ((( x  >> 8 ) & 0xffu ) | (( x  & 0xffu ) << 8 ));
    }
#endif

#if !defined(bswap32)
    static inline uint32_t bswap32(uint32_t x) {
        return ((( x & 0xff000000u ) >> 24 ) |
                (( x & 0x00ff0000u ) >> 8  ) |
                (( x & 0x0000ff00u ) << 8  ) |
                (( x & 0x000000ffu ) << 24 ));
    }
#endif

#if !defined(bswap64)
    static inline uint64_t bswap64(uint64_t x) {
        return ((( x & 0xff00000000000000ull ) >> 56 ) |
                (( x & 0x00ff000000000000ull ) >> 40 ) |
                (( x & 0x0000ff0000000000ull ) >> 24 ) |
                (( x & 0x000000ff00000000ull ) >> 8  ) |
                (( x & 0x00000000ff000000ull ) << 8  ) |
                (( x & 0x0000000000ff0000ull ) << 24 ) |
                (( x & 0x000000000000ff00ull ) << 40 ) |
                (( x & 0x00000000000000ffull ) << 56 ));
    }
#endif

//! Byte-swap 32-bit float
static inline float bswapf(float f) {
#ifdef __cplusplus
    static_assert(sizeof(float) == sizeof(uint32_t), "Unexpected float format");
    /* Problem: de-referencing float pointer as uint32_t breaks strict-aliasing rules for C++ and C, even if it normally works
     *   uint32_t val = bswap32(*(reinterpret_cast<const uint32_t *>(&f)));
     *   return *(reinterpret_cast<float *>(&val));
     */
    // memcpy approach is guaranteed to work in C & C++ and fn calls should be optimized out:
    uint32_t asInt;
    std::memcpy(&asInt, reinterpret_cast<const void *>(&f), sizeof(uint32_t));
    asInt = bswap32(asInt);
    std::memcpy(&f, reinterpret_cast<void *>(&asInt), sizeof(float));
    return f;
#else
    _Static_assert(sizeof(float) == sizeof(uint32_t), "Unexpected float format");
    // union approach is guaranteed to work in C99 and later (but not in C++, though in practice it normally will):
    union { uint32_t asInt; float asFloat; } conversion_union;
    conversion_union.asFloat = f;
    conversion_union.asInt = bswap32(conversion_union.asInt);
    return conversion_union.asFloat;
#endif
}

//! Byte-swap 64-bit double
static inline double bswapd(double d) {
#ifdef __cplusplus
    static_assert(sizeof(double) == sizeof(uint64_t), "Unexpected double format");
    uint64_t asInt;
    std::memcpy(&asInt, reinterpret_cast<const void *>(&d), sizeof(uint64_t));
    asInt = bswap64(asInt);
    std::memcpy(&d, reinterpret_cast<void *>(&asInt), sizeof(double));
    return d;
#else
    _Static_assert(sizeof(double) == sizeof(uint64_t), "Unexpected double format");
    union { uint64_t asInt; double asDouble; } conversion_union;
    conversion_union.asDouble = d;
    conversion_union.asInt = bswap64(conversion_union.asInt);
    return conversion_union.asDouble;
#endif
}


/* Define network - host byte swaps as needed depending upon platform endianness */
// (note that network order is big endian)

#if defined(__LITTLE_ENDIAN__)
#  define ntoh16(x)     bswap16((x))
#  define hton16(x)     bswap16((x))
#  define ntoh32(x)     bswap32((x))
#  define hton32(x)     bswap32((x))
#  define ntoh64(x)     bswap64((x))
#  define hton64(x)     bswap64((x))
#  define ntohf(x)      bswapf((x))
#  define htonf(x)      bswapf((x))
#  define ntohd(x)      bswapd((x))
#  define htond(x)      bswapd((x))
#  define btoh16(x)     bswap16((x))
#  define htob16(x)     bswap16((x))
#  define btoh32(x)     bswap32((x))
#  define htob32(x)     bswap32((x))
#  define btoh64(x)     bswap64((x))
#  define htob64(x)     bswap64((x))
#  define btohf(x)      bswapf((x))
#  define htobf(x)      bswapf((x))
#  define btohd(x)      bswapd((x))
#  define htobd(x)      bswapd((x))
#  define ltoh16(x)     (x)
#  define htol16(x)     (x)
#  define ltoh32(x)     (x)
#  define htol32(x)     (x)
#  define ltoh64(x)     (x)
#  define htol64(x)     (x)
#  define ltohf(x)      (x)
#  define htolf(x)      (x)
#  define ltohd(x)      (x)
#  define htold(x)      (x)
#elif defined(__BIG_ENDIAN__)
#  define ntoh16(x)     (x)
#  define hton16(x)     (x)
#  define ntoh32(x)     (x)
#  define hton32(x)     (x)
#  define ntoh64(x)     (x)
#  define hton64(x)     (x)
#  define ntohf(x)      (x)
#  define htonf(x)      (x)
#  define ntohd(x)      (x)
#  define htond(x)      (x)
#  define btoh16(x)     (x)
#  define htob16(x)     (x)
#  define btoh32(x)     (x)
#  define htob32(x)     (x)
#  define btoh64(x)     (x)
#  define htob64(x)     (x)
#  define btohf(x)      (x)
#  define htobf(x)      (x)
#  define btohd(x)      (x)
#  define htobd(x)      (x)
#  define ltoh16(x)     bswap16((x))
#  define htol16(x)     bswap16((x))
#  define ltoh32(x)     bswap32((x))
#  define htol32(x)     bswap32((x))
#  define ltoh64(x)     bswap64((x))
#  define htol64(x)     bswap64((x))
#  define ltohf(x)      bswapf((x))
#  define htolf(x)      bswapf((x))
#  define ltohd(x)      bswapd((x))
#  define htold(x)      bswapd((x))
#  else
#    warning "UNKNOWN Platform / endianness; network / host byte swaps not defined."
#endif

#if defined(__cplusplus) && (defined(__LITTLE_ENDIAN__) || defined(__BIG_ENDIAN__))
static inline uint16_t ntoh(uint16_t x) { return ntoh16(x); }
static inline uint16_t hton(uint16_t x) { return hton16(x); }
static inline uint32_t ntoh(uint32_t x) { return ntoh32(x); }
static inline uint32_t hton(uint32_t x) { return hton32(x); }
static inline uint64_t ntoh(uint64_t x) { return ntoh64(x); }
static inline uint64_t hton(uint64_t x) { return hton64(x); }
static inline float    ntoh(float    x) { return ntohf (x); }
static inline float    hton(float    x) { return htonf (x); }
static inline double   ntoh(double   x) { return ntohd (x); }
static inline double   hton(double   x) { return htond (x); }
static inline uint16_t btoh(uint16_t x) { return btoh16(x); }
static inline uint16_t htob(uint16_t x) { return htob16(x); }
static inline uint32_t btoh(uint32_t x) { return btoh32(x); }
static inline uint32_t htob(uint32_t x) { return htob32(x); }
static inline uint64_t btoh(uint64_t x) { return btoh64(x); }
static inline uint64_t htob(uint64_t x) { return htob64(x); }
static inline float    btoh(float    x) { return btohf (x); }
static inline float    htob(float    x) { return htobf (x); }
static inline double   btoh(double   x) { return btohd (x); }
static inline double   htob(double   x) { return htobd (x); }
static inline uint16_t ltoh(uint16_t x) { return ltoh16(x); }
static inline uint16_t htol(uint16_t x) { return htol16(x); }
static inline uint32_t ltoh(uint32_t x) { return ltoh32(x); }
static inline uint32_t htol(uint32_t x) { return htol32(x); }
static inline uint64_t ltoh(uint64_t x) { return ltoh64(x); }
static inline uint64_t htol(uint64_t x) { return htol64(x); }
static inline float    ltoh(float    x) { return ltohf (x); }
static inline float    htol(float    x) { return htolf (x); }
static inline double   ltoh(double   x) { return ltohd (x); }
static inline double   htol(double   x) { return htold (x); }
#endif

#endif //ENDIANNESS_H_