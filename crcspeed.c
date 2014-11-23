/*
 * Copyright (C) 2013 Mark Adler
 * Originally by: crc64.c Version 1.4  16 Dec 2013  Mark Adler
 * Modifications by Matt Stancliff <matt@genges.com>:
 *   - removed CRC64-specific behavior
 *   - added generation of lookup tables by parameters
 *   - removed inversion of CRC input/result
 *   - removed automatic initialization in favor of explicit initialization

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
 */

#include "crcspeed.h"

/* Fill in a CRC constants table. */
static void crc_init64(crcfn64 crcfn, uint64_t table[8][256]) {
    uint64_t crc;

    /* generate CRC's for all single byte sequences */
    for (int n = 0; n < 256; n++) {
        crc = n;
        table[0][n] = crcfn(0, &crc, 1);
    }

    /* generate CRC's for those followed by 1 to 7 zero bytes */
    for (int n = 0; n < 256; n++) {
        crc = table[0][n];
        for (int k = 1; k < 8; k++) {
            crc = table[0][crc & 0xff] ^ (crc >> 8);
            table[k][n] = crc;
        }
    }
}

/* Reverse the bytes in a 64-bit word. */
static inline uint64_t rev8(uint64_t a) {
    uint64_t m;

    m = UINT64_C(0xff00ff00ff00ff);
    a = ((a >> 8) & m) | (a & m) << 8;
    m = UINT64_C(0xffff0000ffff);
    a = ((a >> 16) & m) | (a & m) << 16;
    return a >> 32 | a << 32;
}

/* This function is called once to initialize the CRC table for use on a
   big-endian architecture. */
static void big_init64(crcfn64 fn, uint64_t big_table[8][256]) {
    crc_init64(fn, big_table);
    for (int k = 0; k < 8; k++)
        for (int n = 0; n < 256; n++)
            big_table[k][n] = rev8(big_table[k][n]);
}

/* Calculate a non-inverted CRC multiple bytes at a time on a little-endian
 * architecture. If you need inverted CRC, invert *before* calling and invert
 * *after* calling.
 * 64 bit crc = process 8 bytes at once;
 */
static uint64_t little64(uint64_t little_table[8][256], uint64_t crc, void *buf,
                         size_t len) {
    unsigned char *next = buf;

    while (len && ((uintptr_t)next & 7) != 0) {
        crc = little_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    /* fast middle processing */
    while (len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = little_table[7][crc & 0xff] ^
              little_table[6][(crc >> 8) & 0xff] ^
              little_table[5][(crc >> 16) & 0xff] ^
              little_table[4][(crc >> 24) & 0xff] ^
              little_table[3][(crc >> 32) & 0xff] ^
              little_table[2][(crc >> 40) & 0xff] ^
              little_table[1][(crc >> 48) & 0xff] ^
              little_table[0][crc >> 56];
        next += 8;
        len -= 8;
    }
    while (len) {
        crc = little_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    return crc;
}

/* Calculate a non-inverted CRC eight bytes at a time on a big-endian
 * architecture.
 */
static uint64_t big64(uint64_t big_table[8][256], uint64_t crc, void *buf,
                      size_t len) {
    unsigned char *next = buf;

    crc = rev8(crc);
    while (len && ((uintptr_t)next & 7) != 0) {
        crc = big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }
    while (len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = big_table[0][crc & 0xff] ^
              big_table[1][(crc >> 8) & 0xff] ^
              big_table[2][(crc >> 16) & 0xff] ^
              big_table[3][(crc >> 24) & 0xff] ^
              big_table[4][(crc >> 32) & 0xff] ^
              big_table[5][(crc >> 40) & 0xff] ^
              big_table[6][(crc >> 48) & 0xff] ^
              big_table[7][crc >> 56];
        next += 8;
        len -= 8;
    }
    while (len) {
        crc = big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }
    return rev8(crc);
}

/* Return the CRC of buf[0..len-1] with initial crc, processing eight bytes
   at a time using passed-in lookup table.
   This selects one of two routines depending on the endianess of
   the architecture.  A good optimizing compiler will determine the endianess
   at compile time if it can, and get rid of the unused code and table.  If the
   endianess can be changed at run time, then this code will handle that as
   well, initializing and using two tables, if called upon to do so. */
uint64_t crcspeed64(uint64_t table[8][256], uint64_t crc, void *buf,
                    size_t len) {
    uint64_t n = 1;

    return *(char *)&n ? little64(table, crc, buf, len)
                       : big64(table, crc, buf, len);
}

/* Initialize CRC lookup table in architecture-dependent manner. */
void crcspeed_init64(crcfn64 fn, uint64_t table[8][256]) {
    uint64_t n = 1;

    *(char *)&n ? crc_init64(fn, table) : big_init64(fn, table);
}