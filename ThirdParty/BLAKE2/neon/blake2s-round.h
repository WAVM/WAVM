/*
   BLAKE2 reference source code package - optimized C implementations

   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:

   - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0

   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
*/
#ifndef BLAKE2S_ROUND_H
#define BLAKE2S_ROUND_H

#define vrorq_n_u32_16(x) vreinterpretq_u32_u16(	\
			  vrev32q_u16(			\
			  vreinterpretq_u16_u32(x)))

#define vrorq_n_u32_12(x) vorrq_u32(			\
			  vshrq_n_u32(x, 12),		\
			  vshlq_n_u32(x, 20));

#define vrorq_n_u32_8(x)  vorrq_u32(			\
			  vshrq_n_u32(x,  8),		\
			  vshlq_n_u32(x, 24));

#define vrorq_n_u32_7(x)  vorrq_u32(			\
			  vshrq_n_u32(x,  7),		\
			  vshlq_n_u32(x, 25));

#define G1(row1,row2,row3,row4,buf) \
  row1 = vaddq_u32(row1, vaddq_u32(row2, buf)); \
  row4 = vrorq_n_u32_16(veorq_u32(row4, row1)); \
  row3 = vaddq_u32(row3, row4); \
  row2 = vrorq_n_u32_12(veorq_u32(row2, row3));

#define G2(row1, row2, row3, row4,buf) \
  row1 = vaddq_u32(row1, vaddq_u32(row2, buf)); \
  row4 = vrorq_n_u32_8(veorq_u32(row4, row1)); \
  row3 = vaddq_u32(row3, row4); \
  row2 = vrorq_n_u32_7(veorq_u32(row2, row3));

#define DIAGONALIZE(row1,row2,row3,row4) \
  row2 = vextq_u32(row2, row2, 1); \
  row3 = vextq_u32(row3, row3, 2); \
  row4 = vextq_u32(row4, row4, 3);

#define UNDIAGONALIZE(row1,row2,row3,row4) \
  row2 = vextq_u32(row2, row2, 3); \
  row3 = vextq_u32(row3, row3, 2); \
  row4 = vextq_u32(row4, row4, 1);

#include "blake2s-load-neon.h"

#define ROUND(r) \
  LOAD_MSG_ ##r ##_1(buf1); \
  G1(row1, row2, row3, row4, buf1); \
  LOAD_MSG_ ##r ##_2(buf2); \
  G2(row1, row2, row3, row4, buf2); \
  DIAGONALIZE(row1, row2, row3, row4); \
  LOAD_MSG_ ##r ##_3(buf3); \
  G1(row1, row2, row3, row4, buf3); \
  LOAD_MSG_ ##r ##_4(buf4); \
  G2(row1, row2, row3, row4, buf4); \
  UNDIAGONALIZE(row1, row2, row3, row4);
    
#endif
