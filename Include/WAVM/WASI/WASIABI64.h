#pragma once

#include "WASIABI.h"

typedef uint64_t __wasi_void_ptr_i64;
typedef uint64_t __wasi_size_i64;

typedef struct __wasi_prestat_i64
{
	__wasi_preopentype_t pr_type;
	union __wasi_prestat_u
	{
		struct __wasi_prestat_u_dir_t
		{
			__wasi_size_i64 pr_name_len;
		} dir;
	} u;
} __wasi_prestat_i64;

typedef struct __wasi_ciovec_i64
{
	__wasi_void_ptr_i64 buf;
	__wasi_size_i64 buf_len;
} __wasi_ciovec_i64;

typedef struct __wasi_iovec_i64
{
	__wasi_void_ptr_i64 buf;
	__wasi_size_i64 buf_len;
} __wasi_iovec_i64;
