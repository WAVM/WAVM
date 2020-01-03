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
#ifndef BLAKE2S_LOAD_NEON_H
#define BLAKE2S_LOAD_NEON_H

#define LOAD_MSG_0_1(buf)						\
  t1 = vzip_u32(m0, m1);						\
  t2 = vzip_u32(m2, m3);						\
  buf = vcombine_u32(t1.val[0], t2.val[0]);

#define LOAD_MSG_0_2(buf)						\
  t1 = vzip_u32(m0, m1);						\
  t2 = vzip_u32(m2, m3);						\
  buf = vcombine_u32(t1.val[1], t2.val[1]);

#define LOAD_MSG_0_3(buf)						\
  t1 = vzip_u32(m4, m5);						\
  t2 = vzip_u32(m6, m7);						\
  buf = vcombine_u32(t1.val[0], t2.val[0]);

#define LOAD_MSG_0_4(buf)						\
  t1 = vzip_u32(m4, m5);						\
  t2 = vzip_u32(m6, m7);						\
  buf = vcombine_u32(t1.val[1], t2.val[1]);

#define LOAD_MSG_1_1(buf)						\
  t1 = vzip_u32(m7, m2);						\
  t2 = vzip_u32(m4, m6);						\
  buf = vcombine_u32(t1.val[0], t2.val[1]);

#define LOAD_MSG_1_2(buf)						\
  t1 = vzip_u32(m5, m4);						\
  buf = vcombine_u32(t1.val[0], vext_u32(m7, m3, 1));

#define LOAD_MSG_1_3(buf)						\
  t2 = vzip_u32(m5, m2);						\
  buf = vcombine_u32(vrev64_u32(m0), t2.val[1]);

#define LOAD_MSG_1_4(buf)						\
  t1 = vzip_u32(m6, m1);						\
  t2 = vzip_u32(m3, m1);						\
  buf = vcombine_u32(t1.val[0], t2.val[1]);

#define LOAD_MSG_2_1(buf)						\
  t2 = vzip_u32(m2, m7);						\
  buf = vcombine_u32(vext_u32(m5, m6, 1), t2.val[1]);

#define LOAD_MSG_2_2(buf)						\
  t1 = vzip_u32(m4, m0);						\
  buf = vcombine_u32(t1.val[0], vrev64_u32(vext_u32(m6, m1, 1)));

#define LOAD_MSG_2_3(buf)						\
  t2 = vzip_u32(m3, m4);						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m1, m5, 1)), t2.val[1]);

#define LOAD_MSG_2_4(buf)						\
  t1 = vzip_u32(m7, m3);						\
  buf = vcombine_u32(t1.val[0], vext_u32(m0, m2, 1));

#define LOAD_MSG_3_1(buf)						\
  t1 = vzip_u32(m3, m1);						\
  t2 = vzip_u32(m6, m5);						\
  buf = vcombine_u32(t1.val[1], t2.val[1]);

#define LOAD_MSG_3_2(buf)						\
  t1 = vzip_u32(m4, m0);						\
  t2 = vzip_u32(m6, m7);						\
  buf = vcombine_u32(t1.val[1], t2.val[0]);

#define LOAD_MSG_3_3(buf)						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m2, m1, 1)),			\
		     vrev64_u32(vext_u32(m7, m2, 1)));

#define LOAD_MSG_3_4(buf)						\
  t1 = vzip_u32(m3, m5);						\
  t2 = vzip_u32(m0, m4);						\
  buf = vcombine_u32(t1.val[0], t2.val[0]);

#define LOAD_MSG_4_1(buf)						\
  t1 = vzip_u32(m4, m2);						\
  t2 = vzip_u32(m1, m5);						\
  buf = vcombine_u32(t1.val[1], t2.val[0]);

#define LOAD_MSG_4_2(buf)						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m3, m0, 1)),			\
		     vrev64_u32(vext_u32(m7, m2, 1)));

#define LOAD_MSG_4_3(buf)						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m5, m7, 1)),			\
		     vrev64_u32(vext_u32(m1, m3, 1)));

#define LOAD_MSG_4_4(buf)						\
  buf = vcombine_u32(vext_u32(m0, m6, 1),				\
		     vrev64_u32(vext_u32(m6, m4, 1)));

#define LOAD_MSG_5_1(buf)						\
  t1 = vzip_u32(m1, m3);						\
  t2 = vzip_u32(m0, m4);						\
  buf = vcombine_u32(t1.val[0], t2.val[0]);

#define LOAD_MSG_5_2(buf)						\
  t1 = vzip_u32(m6, m5);						\
  t2 = vzip_u32(m5, m1);						\
  buf = vcombine_u32(t1.val[0], t2.val[1]);

#define LOAD_MSG_5_3(buf)						\
  t2 = vzip_u32(m7, m0);						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m3, m2, 1)), t2.val[1]);

#define LOAD_MSG_5_4(buf)						\
  t1 = vzip_u32(m6, m2);						\
  buf = vcombine_u32(t1.val[1], vrev64_u32(vext_u32(m4, m7, 1)));

#define LOAD_MSG_6_1(buf)						\
  t2 = vzip_u32(m7, m2);						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m0, m6, 1)), t2.val[0]);

#define LOAD_MSG_6_2(buf)						\
  t1 = vzip_u32(m2, m7);						\
  buf = vcombine_u32(t1.val[1], vext_u32(m6, m5, 1));

#define LOAD_MSG_6_3(buf)						\
  t1 = vzip_u32(m0, m3);						\
  buf = vcombine_u32(t1.val[0], vrev64_u32(m4));

#define LOAD_MSG_6_4(buf)						\
  t1 = vzip_u32(m3, m1);						\
  buf = vcombine_u32(t1.val[1], vrev64_u32(vext_u32(m5, m1, 1)));

#define LOAD_MSG_7_1(buf)						\
  t1 = vzip_u32(m6, m3);						\
  buf = vcombine_u32(t1.val[1], vrev64_u32(vext_u32(m1, m6, 1)));

#define LOAD_MSG_7_2(buf)						\
  t2 = vzip_u32(m0, m4);						\
  buf = vcombine_u32(vext_u32(m5, m7, 1), t2.val[1]);

#define LOAD_MSG_7_3(buf)						\
  t1 = vzip_u32(m2, m7);						\
  t2 = vzip_u32(m4, m1);						\
  buf = vcombine_u32(t1.val[1], t2.val[0]);

#define LOAD_MSG_7_4(buf)						\
  t1 = vzip_u32(m0, m2);						\
  t2 = vzip_u32(m3, m5);						\
  buf = vcombine_u32(t1.val[0], t2.val[0]);

#define LOAD_MSG_8_1(buf)						\
  t1 = vzip_u32(m3, m7);						\
  buf = vcombine_u32(t1.val[0], vext_u32(m5, m0, 1));

#define LOAD_MSG_8_2(buf)						\
  t1 = vzip_u32(m7, m4);						\
  buf = vcombine_u32(t1.val[1], vext_u32(m1, m4, 1));

#define LOAD_MSG_8_3(buf)						\
  buf = vcombine_u32(m6, vext_u32(m0, m5, 1));

#define LOAD_MSG_8_4(buf)						\
  buf = vcombine_u32(vrev64_u32(vext_u32(m3, m1, 1)), m2);

#define LOAD_MSG_9_1(buf)						\
  t1 = vzip_u32(m5, m4);						\
  t2 = vzip_u32(m3, m0);						\
  buf = vcombine_u32(t1.val[0], t2.val[1]);

#define LOAD_MSG_9_2(buf)						\
  t1 = vzip_u32(m1, m2);						\
  buf = vcombine_u32(t1.val[0], vrev64_u32(vext_u32(m2, m3, 1)));

#define LOAD_MSG_9_3(buf)						\
  t1 = vzip_u32(m7, m4);						\
  t2 = vzip_u32(m1, m6);						\
  buf = vcombine_u32(t1.val[1], t2.val[1]);

#define LOAD_MSG_9_4(buf)						\
  t2 = vzip_u32(m6, m0);						\
  buf = vcombine_u32(vext_u32(m5, m7, 1), t2.val[0]);


#endif
