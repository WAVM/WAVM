/*
   BLAKE2 reference source code package - reference C implementations

   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:

   - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0

   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
*/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <arm_neon.h>

#include "blake2.h"
#include "blake2-impl.h"

static const uint32_t blake2s_IV[8] =
{
  0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
  0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static void blake2s_set_lastnode( blake2s_state *S )
{
  S->f[1] = (uint32_t)-1;
}

/* Some helper functions, not necessarily useful */
static int blake2s_is_lastblock( const blake2s_state *S )
{
  return S->f[0] != 0;
}

static void blake2s_set_lastblock( blake2s_state *S )
{
  if( S->last_node ) blake2s_set_lastnode( S );

  S->f[0] = (uint32_t)-1;
}

static void blake2s_increment_counter( blake2s_state *S, const uint32_t inc )
{
  S->t[0] += inc;
  S->t[1] += ( S->t[0] < inc );
}

static void blake2s_init0( blake2s_state *S )
{
  size_t i;
  memset( S, 0, sizeof( blake2s_state ) );

  for( i = 0; i < 8; ++i ) S->h[i] = blake2s_IV[i];
}

/* init2 xors IV with input parameter block */
int blake2s_init_param( blake2s_state *S, const blake2s_param *P )
{
  const unsigned char *p = ( const unsigned char * )( P );
  size_t i;

  blake2s_init0( S );

  /* IV XOR ParamBlock */
  for( i = 0; i < 8; ++i )
    S->h[i] ^= load32( &p[i * 4] );

  S->outlen = P->digest_length;
  return 0;
}


/* Sequential blake2s initialization */
int blake2s_init( blake2s_state *S, size_t outlen )
{
  blake2s_param P[1];

  /* Move interval verification here? */
  if ( ( !outlen ) || ( outlen > BLAKE2S_OUTBYTES ) ) return -1;

  P->digest_length = (uint8_t)outlen;
  P->key_length    = 0;
  P->fanout        = 1;
  P->depth         = 1;
  store32( &P->leaf_length, 0 );
  store32( &P->node_offset, 0 );
  store16( &P->xof_length, 0 );
  P->node_depth    = 0;
  P->inner_length  = 0;
  /* memset(P->reserved, 0, sizeof(P->reserved) ); */
  memset( P->salt,     0, sizeof( P->salt ) );
  memset( P->personal, 0, sizeof( P->personal ) );
  return blake2s_init_param( S, P );
}

int blake2s_init_key( blake2s_state *S, size_t outlen, const void *key, size_t keylen )
{
  blake2s_param P[1];

  if ( ( !outlen ) || ( outlen > BLAKE2S_OUTBYTES ) ) return -1;

  if ( !key || !keylen || keylen > BLAKE2S_KEYBYTES ) return -1;

  P->digest_length = (uint8_t)outlen;
  P->key_length    = (uint8_t)keylen;
  P->fanout        = 1;
  P->depth         = 1;
  store32( &P->leaf_length, 0 );
  store32( &P->node_offset, 0 );
  store16( &P->xof_length, 0 );
  P->node_depth    = 0;
  P->inner_length  = 0;
  /* memset(P->reserved, 0, sizeof(P->reserved) ); */
  memset( P->salt,     0, sizeof( P->salt ) );
  memset( P->personal, 0, sizeof( P->personal ) );

  if( blake2s_init_param( S, P ) < 0 ) return -1;

  {
    uint8_t block[BLAKE2S_BLOCKBYTES];
    memset( block, 0, BLAKE2S_BLOCKBYTES );
    memcpy( block, key, keylen );
    blake2s_update( S, block, BLAKE2S_BLOCKBYTES );
    secure_zero_memory( block, BLAKE2S_BLOCKBYTES ); /* Burn the key from stack */
  }
  return 0;
}

/* Round 0 */
#undef  LOAD_MSG_0_1_
#define LOAD_MSG_0_1_(x)						\
  do {									\
    t1 = vzip_u32(m0, m1);						\
    t2 = vzip_u32(m2, m3);						\
    x = vcombine_u32(t1.val[0], t2.val[0]);				\
  } while(0)

#undef  LOAD_MSG_0_2_
#define LOAD_MSG_0_2_(x)						\
  do {									\
    t1 = vzip_u32(m0, m1);						\
    t2 = vzip_u32(m2, m3);						\
    x = vcombine_u32(t1.val[1], t2.val[1]);				\
  } while(0)

#undef  LOAD_MSG_0_3_
#define LOAD_MSG_0_3_(x)						\
  do {									\
    t1 = vzip_u32(m4, m5);						\
    t2 = vzip_u32(m6, m7);						\
    x = vcombine_u32(t1.val[0], t2.val[0]);				\
  } while(0)

#undef  LOAD_MSG_0_4_
#define LOAD_MSG_0_4_(x)						\
  do {									\
    t1 = vzip_u32(m4, m5);						\
    t2 = vzip_u32(m6, m7);						\
    x = vcombine_u32(t1.val[1], t2.val[1]);				\
  } while(0)

/* Round 1 */
#undef  LOAD_MSG_1_1_
#define LOAD_MSG_1_1_(x)						\
  do {									\
    t1 = vzip_u32(m7, m2);						\
    t2 = vzip_u32(m4, m6);						\
    x = vcombine_u32(t1.val[0], t2.val[1]);				\
  } while(0)

#undef  LOAD_MSG_1_2_
#define LOAD_MSG_1_2_(x)						\
  do {									\
    t1 = vzip_u32(m5, m4);						\
    x = vcombine_u32(t1.val[0], vext_u32(m7, m3, 1));			\
  } while(0)

#undef  LOAD_MSG_1_3_
#define LOAD_MSG_1_3_(x)						\
  do {									\
    t2 = vzip_u32(m5, m2);						\
    x = vcombine_u32(vrev64_u32(m0), t2.val[1]);			\
  } while(0)

#undef  LOAD_MSG_1_4_
#define LOAD_MSG_1_4_(x)						\
  do {									\
    t1 = vzip_u32(m6, m1);						\
    t2 = vzip_u32(m3, m1);						\
    x = vcombine_u32(t1.val[0], t2.val[1]);				\
  } while(0)

/* Round 2 */
#undef  LOAD_MSG_2_1_
#define LOAD_MSG_2_1_(x)						\
  do {									\
    t2 = vzip_u32(m2, m7);						\
    x = vcombine_u32(vext_u32(m5, m6, 1), t2.val[1]);			\
  } while(0)

#undef  LOAD_MSG_2_2_
#define LOAD_MSG_2_2_(x)						\
  do {									\
    t1 = vzip_u32(m4, m0);						\
    x = vcombine_u32(t1.val[0], vrev64_u32(vext_u32(m6, m1, 1)));	\
  } while(0)

#undef  LOAD_MSG_2_3_
#define LOAD_MSG_2_3_(x)						\
  do {									\
    t2 = vzip_u32(m3, m4);						\
    x = vcombine_u32(vrev64_u32(vext_u32(m1, m5, 1)), t2.val[1]);	\
  } while(0)

#undef  LOAD_MSG_2_4_
#define LOAD_MSG_2_4_(x)						\
  do {									\
    t1 = vzip_u32(m7, m3);						\
    x = vcombine_u32(t1.val[0], vext_u32(m0, m2, 1));			\
  } while(0)

/* Round 3 */
#undef  LOAD_MSG_3_1_
#define LOAD_MSG_3_1_(x)						\
  do {									\
    t1 = vzip_u32(m3, m1);						\
    t2 = vzip_u32(m6, m5);						\
    x = vcombine_u32(t1.val[1], t2.val[1]);				\
  } while(0)

#undef  LOAD_MSG_3_2_
#define LOAD_MSG_3_2_(x)						\
  do {									\
    t1 = vzip_u32(m4, m0);						\
    t2 = vzip_u32(m6, m7);						\
    x = vcombine_u32(t1.val[1], t2.val[0]);				\
  } while(0)

#undef  LOAD_MSG_3_3_
#define LOAD_MSG_3_3_(x)						\
  do {									\
    x = vcombine_u32(vrev64_u32(vext_u32(m2, m1, 1)),			\
		     vrev64_u32(vext_u32(m7, m2, 1)));			\
  } while(0)

#undef  LOAD_MSG_3_4_
#define LOAD_MSG_3_4_(x)						\
  do {									\
    t1 = vzip_u32(m3, m5);						\
    t2 = vzip_u32(m0, m4);						\
    x = vcombine_u32(t1.val[0], t2.val[0]);				\
  } while(0)

/* Round 4 */
#undef  LOAD_MSG_4_1_
#define LOAD_MSG_4_1_(x)						\
  do {									\
    t1 = vzip_u32(m4, m2);						\
    t2 = vzip_u32(m1, m5);						\
    x = vcombine_u32(t1.val[1], t2.val[0]);				\
  } while(0)

#undef  LOAD_MSG_4_2_
#define LOAD_MSG_4_2_(x)						\
  do {									\
    x = vcombine_u32(vrev64_u32(vext_u32(m3, m0, 1)),			\
		     vrev64_u32(vext_u32(m7, m2, 1)));			\
  } while(0)

#undef  LOAD_MSG_4_3_
#define LOAD_MSG_4_3_(x)						\
  do {									\
    x = vcombine_u32(vrev64_u32(vext_u32(m5, m7, 1)),			\
		     vrev64_u32(vext_u32(m1, m3, 1)));			\
  } while(0)

#undef  LOAD_MSG_4_4_
#define LOAD_MSG_4_4_(x)						\
  do {									\
    x = vcombine_u32(vext_u32(m0, m6, 1),				\
		     vrev64_u32(vext_u32(m6, m4, 1)));			\
  } while(0)

/* Round 5 */
#undef  LOAD_MSG_5_1_
#define LOAD_MSG_5_1_(x)						\
  do {									\
    t1 = vzip_u32(m1, m3);						\
    t2 = vzip_u32(m0, m4);						\
    x = vcombine_u32(t1.val[0], t2.val[0]);				\
  } while(0)

#undef  LOAD_MSG_5_2_
#define LOAD_MSG_5_2_(x)						\
  do {									\
    t1 = vzip_u32(m6, m5);						\
    t2 = vzip_u32(m5, m1);						\
    x = vcombine_u32(t1.val[0], t2.val[1]);				\
  } while(0)

#undef  LOAD_MSG_5_3_
#define LOAD_MSG_5_3_(x)						\
  do {									\
    t2 = vzip_u32(m7, m0);						\
    x = vcombine_u32(vrev64_u32(vext_u32(m3, m2, 1)), t2.val[1]);	\
  } while(0)

#undef  LOAD_MSG_5_4_
#define LOAD_MSG_5_4_(x)						\
  do {									\
    t1 = vzip_u32(m6, m2);						\
    x = vcombine_u32(t1.val[1], vrev64_u32(vext_u32(m4, m7, 1)));	\
  } while(0)

/* Round 6 */
#undef  LOAD_MSG_6_1_
#define LOAD_MSG_6_1_(x)						\
  do {									\
    t2 = vzip_u32(m7, m2);						\
    x = vcombine_u32(vrev64_u32(vext_u32(m0, m6, 1)), t2.val[0]);	\
  } while(0)

#undef  LOAD_MSG_6_2_
#define LOAD_MSG_6_2_(x)						\
  do {									\
    t1 = vzip_u32(m2, m7);						\
    x = vcombine_u32(t1.val[1], vext_u32(m6, m5, 1));			\
  } while(0)

#undef  LOAD_MSG_6_3_
#define LOAD_MSG_6_3_(x)						\
  do {									\
    t1 = vzip_u32(m0, m3);						\
    x = vcombine_u32(t1.val[0], vrev64_u32(m4));			\
  } while(0)

#undef  LOAD_MSG_6_4_
#define LOAD_MSG_6_4_(x)						\
  do {									\
    t1 = vzip_u32(m3, m1);						\
    x = vcombine_u32(t1.val[1], vrev64_u32(vext_u32(m5, m1, 1)));	\
  } while(0)

/* Round 7 */
#undef  LOAD_MSG_7_1_
#define LOAD_MSG_7_1_(x)						\
  do {									\
    t1 = vzip_u32(m6, m3);						\
    x = vcombine_u32(t1.val[1], vrev64_u32(vext_u32(m1, m6, 1)));	\
  } while(0)

#undef  LOAD_MSG_7_2_
#define LOAD_MSG_7_2_(x)						\
  do {									\
    t2 = vzip_u32(m0, m4);						\
    x = vcombine_u32(vext_u32(m5, m7, 1), t2.val[1]);			\
  } while(0)

#undef  LOAD_MSG_7_3_
#define LOAD_MSG_7_3_(x)						\
  do {									\
    t1 = vzip_u32(m2, m7);						\
    t2 = vzip_u32(m4, m1);						\
    x = vcombine_u32(t1.val[1], t2.val[0]);				\
  } while(0)

#undef  LOAD_MSG_7_4_
#define LOAD_MSG_7_4_(x)						\
  do {									\
    t1 = vzip_u32(m0, m2);						\
    t2 = vzip_u32(m3, m5);						\
    x = vcombine_u32(t1.val[0], t2.val[0]);				\
  } while(0)

/* Round 8 */
#undef  LOAD_MSG_8_1_
#define LOAD_MSG_8_1_(x)						\
  do {									\
    t1 = vzip_u32(m3, m7);						\
    x = vcombine_u32(t1.val[0], vext_u32(m5, m0, 1));			\
  } while(0)

#undef  LOAD_MSG_8_2_
#define LOAD_MSG_8_2_(x)						\
  do {									\
    t1 = vzip_u32(m7, m4);						\
    x = vcombine_u32(t1.val[1], vext_u32(m1, m4, 1));			\
  } while(0)

#undef  LOAD_MSG_8_3_
#define LOAD_MSG_8_3_(x)						\
  do {									\
    x = vcombine_u32(m6, vext_u32(m0, m5, 1));				\
  } while(0)

#undef  LOAD_MSG_8_4_
#define LOAD_MSG_8_4_(x)						\
  do {									\
    x = vcombine_u32(vrev64_u32(vext_u32(m3, m1, 1)), m2);		\
  } while(0)

/* Round 9 */
#undef  LOAD_MSG_9_1_
#define LOAD_MSG_9_1_(x)						\
  do {									\
    t1 = vzip_u32(m5, m4);						\
    t2 = vzip_u32(m3, m0);						\
    x = vcombine_u32(t1.val[0], t2.val[1]);				\
  } while(0)

#undef  LOAD_MSG_9_2_
#define LOAD_MSG_9_2_(x)						\
  do {									\
    t1 = vzip_u32(m1, m2);						\
    x = vcombine_u32(t1.val[0], vrev64_u32(vext_u32(m2, m3, 1)));	\
  } while(0)

#undef  LOAD_MSG_9_3_
#define LOAD_MSG_9_3_(x)						\
  do {									\
    t1 = vzip_u32(m7, m4);						\
    t2 = vzip_u32(m1, m6);						\
    x = vcombine_u32(t1.val[1], t2.val[1]);				\
  } while(0)

#undef  LOAD_MSG_9_4_
#define LOAD_MSG_9_4_(x)						\
  do {									\
    t2 = vzip_u32(m6, m0);						\
    x = vcombine_u32(vext_u32(m5, m7, 1), t2.val[0]);			\
  } while(0)

#define vrorq_n_u32_16(x) vreinterpretq_u32_u16(			\
			  vrev32q_u16(					\
			  vreinterpretq_u16_u32(x)))

#define vrorq_n_u32_12(x) vorrq_u32(					\
			  vshrq_n_u32(x, 12),				\
			  vshlq_n_u32(x, 20));

#define vrorq_n_u32_8(x)  vorrq_u32(					\
			  vshrq_n_u32(x,  8),				\
			  vshlq_n_u32(x, 24));

#define vrorq_n_u32_7(x)  vorrq_u32(					\
			  vshrq_n_u32(x,  7),				\
			  vshlq_n_u32(x, 25));

#define DIAGONALIZE(row1, row2, row3, row4)				\
  do {									\
    /* do nothing to row1 */						\
    row2 = vextq_u32(row2, row2, 1);					\
    row3 = vextq_u32(row3, row3, 2);					\
    row4 = vextq_u32(row4, row4, 3);					\
  } while(0)

#define UNDIAGONALIZE(row1, row2, row3, row4)				\
  do {									\
    /* do nothing to row1 */						\
    row2 = vextq_u32(row2, row2, 3);					\
    row3 = vextq_u32(row3, row3, 2);					\
    row4 = vextq_u32(row4, row4, 1);					\
  } while(0)

#define G1(r, i, row1, row2, row3, row4)				\
  do {									\
    LOAD_MSG_##r##_##i##_(e1234);					\
    row1 = vaddq_u32(row1, vaddq_u32(row2, e1234));			\
    row4 = vrorq_n_u32_16(veorq_u32(row4, row1));			\
    row3 = vaddq_u32(row3, row4);					\
    row2 = vrorq_n_u32_12(veorq_u32(row2, row3));			\
  } while(0)


#define G2(r, i, row1, row2, row3, row4)				\
  do {									\
    LOAD_MSG_##r##_##i##_(e1234);					\
    row1 = vaddq_u32(row1, vaddq_u32(row2, e1234));			\
    row4 = vrorq_n_u32_8(veorq_u32(row4, row1));			\
    row3 = vaddq_u32(row3, row4);					\
    row2 = vrorq_n_u32_7(veorq_u32(row2, row3));			\
  } while(0)

#define ROUND(r)							\
  do {									\
    G1(r, 1, row1, row2, row3, row4);					\
    G2(r, 2, row1, row2, row3, row4);					\
    DIAGONALIZE(row1, row2, row3, row4);				\
    G1(r, 3, row1, row2, row3, row4);					\
    G2(r, 4, row1, row2, row3, row4);					\
    UNDIAGONALIZE(row1, row2, row3, row4);				\
  } while(0)
    
static void blake2s_compress( blake2s_state *S, 
		              const uint8_t in[BLAKE2S_BLOCKBYTES] )
{
  uint32x4_t row1, row2, row3, row4, e1234;
  uint32x2x2_t t1, t2;
  const uint32x4_t h1234 = row1 = vld1q_u32(&S->h[0]);
  const uint32x4_t h5678 = row2 = vld1q_u32(&S->h[4]);

  const uint32x2_t m0 = vreinterpret_u32_u8(vld1_u8(&in[ 0])); 
  const uint32x2_t m1 = vreinterpret_u32_u8(vld1_u8(&in[ 8])); 
  const uint32x2_t m2 = vreinterpret_u32_u8(vld1_u8(&in[16])); 
  const uint32x2_t m3 = vreinterpret_u32_u8(vld1_u8(&in[24])); 
  const uint32x2_t m4 = vreinterpret_u32_u8(vld1_u8(&in[32])); 
  const uint32x2_t m5 = vreinterpret_u32_u8(vld1_u8(&in[40])); 
  const uint32x2_t m6 = vreinterpret_u32_u8(vld1_u8(&in[48])); 
  const uint32x2_t m7 = vreinterpret_u32_u8(vld1_u8(&in[56])); 

  row3 = vld1q_u32(&blake2s_IV[0]);

  row4 = veorq_u32(vcombine_u32(vld1_u32(&S->t[0]), vld1_u32(&S->f[0])),
		    vld1q_u32(&blake2s_IV[4]));

  ROUND( 0 );
  ROUND( 1 );
  ROUND( 2 );
  ROUND( 3 );
  ROUND( 4 );
  ROUND( 5 );
  ROUND( 6 );
  ROUND( 7 );
  ROUND( 8 );
  ROUND( 9 );

  vst1q_u32(&S->h[0], veorq_u32(h1234, veorq_u32(row1, row3)));
  vst1q_u32(&S->h[4], veorq_u32(h5678, veorq_u32(row2, row4)));
}

#undef G1234
#undef ROUND

int blake2s_update( blake2s_state *S, const void *pin, size_t inlen )
{
  const unsigned char * in = (const unsigned char *)pin;
  if( inlen > 0 )
  {
    size_t left = S->buflen;
    size_t fill = BLAKE2S_BLOCKBYTES - left;
    if( inlen > fill )
    {
      S->buflen = 0;
      memcpy( S->buf + left, in, fill ); /* Fill buffer */
      blake2s_increment_counter( S, BLAKE2S_BLOCKBYTES );
      blake2s_compress( S, S->buf ); /* Compress */
      in += fill; inlen -= fill;
      while(inlen > BLAKE2S_BLOCKBYTES) {
        blake2s_increment_counter(S, BLAKE2S_BLOCKBYTES);
        blake2s_compress( S, in );
        in += BLAKE2S_BLOCKBYTES;
        inlen -= BLAKE2S_BLOCKBYTES;
      }
    }
    memcpy( S->buf + S->buflen, in, inlen );
    S->buflen += inlen;
  }
  return 0;
}

int blake2s_final( blake2s_state *S, void *out, size_t outlen )
{
  uint8_t buffer[BLAKE2S_OUTBYTES] = {0};
  size_t i;

  if( out == NULL || outlen < S->outlen )
    return -1;

  if( blake2s_is_lastblock( S ) )
    return -1;

  blake2s_increment_counter( S, ( uint32_t )S->buflen );
  blake2s_set_lastblock( S );
  memset( S->buf + S->buflen, 0, BLAKE2S_BLOCKBYTES - S->buflen ); /* Padding */
  blake2s_compress( S, S->buf );

  for( i = 0; i < 8; ++i ) /* Output full hash to temp buffer */
    store32( buffer + sizeof( S->h[i] ) * i, S->h[i] );

  memcpy( out, buffer, outlen );
  secure_zero_memory(buffer, sizeof(buffer));
  return 0;
}

int blake2s( void *out, size_t outlen, const void *in, size_t inlen, const void *key, size_t keylen )
{
  blake2s_state S[1];

  /* Verify parameters */
  if ( NULL == in && inlen > 0 ) return -1;

  if ( NULL == out ) return -1;

  if ( NULL == key && keylen > 0) return -1;

  if( !outlen || outlen > BLAKE2S_OUTBYTES ) return -1;

  if( keylen > BLAKE2S_KEYBYTES ) return -1;

  if( keylen > 0 )
  {
    if( blake2s_init_key( S, outlen, key, keylen ) < 0 ) return -1;
  }
  else
  {
    if( blake2s_init( S, outlen ) < 0 ) return -1;
  }

  blake2s_update( S, ( const uint8_t * )in, inlen );
  blake2s_final( S, out, outlen );
  return 0;
}

#if defined(SUPERCOP)
int crypto_hash( unsigned char *out, unsigned char *in, unsigned long long inlen )
{
  return blake2s( out, BLAKE2S_OUTBYTES, in, inlen, NULL, 0 );
}
#endif

#if defined(BLAKE2S_SELFTEST)
#include <string.h>
#include "blake2-kat.h"
int main( void )
{
  uint8_t key[BLAKE2S_KEYBYTES];
  uint8_t buf[BLAKE2_KAT_LENGTH];
  size_t i, step;

  for( i = 0; i < BLAKE2S_KEYBYTES; ++i )
    key[i] = ( uint8_t )i;

  for( i = 0; i < BLAKE2_KAT_LENGTH; ++i )
    buf[i] = ( uint8_t )i;

  /* Test simple API */
  for( i = 0; i < BLAKE2_KAT_LENGTH; ++i )
  {
    uint8_t hash[BLAKE2S_OUTBYTES];
    blake2s( hash, BLAKE2S_OUTBYTES, buf, i, key, BLAKE2S_KEYBYTES );

    if( 0 != memcmp( hash, blake2s_keyed_kat[i], BLAKE2S_OUTBYTES ) )
    {
      goto fail;
    }
  }

  /* Test streaming API */
  for(step = 1; step < BLAKE2S_BLOCKBYTES; ++step) {
    for (i = 0; i < BLAKE2_KAT_LENGTH; ++i) {
      uint8_t hash[BLAKE2S_OUTBYTES];
      blake2s_state S;
      uint8_t * p = buf;
      size_t mlen = i;
      int err = 0;

      if( (err = blake2s_init_key(&S, BLAKE2S_OUTBYTES, key, BLAKE2S_KEYBYTES)) < 0 ) {
        goto fail;
      }

      while (mlen >= step) {
        if ( (err = blake2s_update(&S, p, step)) < 0 ) {
          goto fail;
        }
        mlen -= step;
        p += step;
      }
      if ( (err = blake2s_update(&S, p, mlen)) < 0) {
        goto fail;
      }
      if ( (err = blake2s_final(&S, hash, BLAKE2S_OUTBYTES)) < 0) {
        goto fail;
      }

      if (0 != memcmp(hash, blake2s_keyed_kat[i], BLAKE2S_OUTBYTES)) {
        goto fail;
      }
    }
  }

  puts( "ok" );
  return 0;
fail:
  puts("error");
  return -1;
}
#endif
