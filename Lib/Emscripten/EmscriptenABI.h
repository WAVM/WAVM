#pragma once

#include <climits>
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace Emscripten { namespace emabi {

	typedef U32 Address;
	static constexpr Address addressMin = 0;
	static constexpr Address addressMax = UINT32_MAX;

	typedef U32 Size;
	static constexpr Size sizeMin = 0;
	static constexpr Size sizeMax = UINT32_MAX;

	typedef I32 Int;
	static constexpr Int intMin = INT32_MIN;
	static constexpr Int intMax = INT32_MAX;

	typedef I32 Long;
	static constexpr Long longMin = INT32_MIN;
	static constexpr Long longMax = INT32_MAX;

	typedef I32 FD;

	struct iovec
	{
		Address address;
		Size numBytes;
	};

	typedef Long time_t;

	struct tm
	{
		Int tm_sec;
		Int tm_min;
		Int tm_hour;
		Int tm_mday;
		Int tm_mon;
		Int tm_year;
		Int tm_wday;
		Int tm_yday;
		Int tm_isdst;
		Long __tm_gmtoff;
		Address __tm_zone;
	};

	typedef U32 pthread_t;
	typedef U32 pthread_key_t;

	typedef Int Result;
	static constexpr Int resultMin = intMin;
	static constexpr Int resultMax = intMax;

	static constexpr Result esuccess = 0;
	static constexpr Result eperm = -1;
	static constexpr Result enoent = -2;
	static constexpr Result esrch = -3;
	static constexpr Result eintr = -4;
	static constexpr Result eio = -5;
	static constexpr Result enxio = -6;
	static constexpr Result e2big = -7;
	static constexpr Result enoexec = -8;
	static constexpr Result ebadf = -9;
	static constexpr Result echild = -10;
	static constexpr Result eagain = -11;
	static constexpr Result enomem = -12;
	static constexpr Result eacces = -13;
	static constexpr Result efault = -14;
	static constexpr Result enotblk = -15;
	static constexpr Result ebusy = -16;
	static constexpr Result eexist = -17;
	static constexpr Result exdev = -18;
	static constexpr Result enodev = -19;
	static constexpr Result enotdir = -20;
	static constexpr Result eisdir = -21;
	static constexpr Result einval = -22;
	static constexpr Result enfile = -23;
	static constexpr Result emfile = -24;
	static constexpr Result enotty = -25;
	static constexpr Result etxtbsy = -26;
	static constexpr Result efbig = -27;
	static constexpr Result enospc = -28;
	static constexpr Result espipe = -29;
	static constexpr Result erofs = -30;
	static constexpr Result emlink = -31;
	static constexpr Result epipe = -32;
	static constexpr Result edom = -33;
	static constexpr Result erange = -34;
	static constexpr Result edeadlk = -35;
	static constexpr Result enametoolong = -36;
	static constexpr Result enolck = -37;
	static constexpr Result enosys = -38;
	static constexpr Result enotempty = -39;
	static constexpr Result eloop = -40;
	static constexpr Result ewouldblock = eagain;
	static constexpr Result enomsg = -42;
	static constexpr Result eidrm = -43;
	static constexpr Result echrng = -44;
	static constexpr Result el2nsync = -45;
	static constexpr Result el3hlt = -46;
	static constexpr Result el3rst = -47;
	static constexpr Result elnrng = -48;
	static constexpr Result eunatch = -49;
	static constexpr Result enocsi = -50;
	static constexpr Result el2hlt = -51;
	static constexpr Result ebade = -52;
	static constexpr Result ebadr = -53;
	static constexpr Result exfull = -54;
	static constexpr Result enoano = -55;
	static constexpr Result ebadrqc = -56;
	static constexpr Result ebadslt = -57;
	static constexpr Result edeadlock = edeadlk;
	static constexpr Result ebfont = -59;
	static constexpr Result enostr = -60;
	static constexpr Result enodata = -61;
	static constexpr Result etime = -62;
	static constexpr Result enosr = -63;
	static constexpr Result enonet = -64;
	static constexpr Result enopkg = -65;
	static constexpr Result eremote = -66;
	static constexpr Result enolink = -67;
	static constexpr Result eadv = -68;
	static constexpr Result esrmnt = -69;
	static constexpr Result ecomm = -70;
	static constexpr Result eproto = -71;
	static constexpr Result emultihop = -72;
	static constexpr Result edotdot = -73;
	static constexpr Result ebadmsg = -74;
	static constexpr Result eoverflow = -75;
	static constexpr Result enotuniq = -76;
	static constexpr Result ebadfd = -77;
	static constexpr Result eremchg = -78;
	static constexpr Result elibacc = -79;
	static constexpr Result elibbad = -80;
	static constexpr Result elibscn = -81;
	static constexpr Result elibmax = -82;
	static constexpr Result elibexec = -83;
	static constexpr Result eilseq = -84;
	static constexpr Result erestart = -85;
	static constexpr Result estrpipe = -86;
	static constexpr Result eusers = -87;
	static constexpr Result enotsock = -88;
	static constexpr Result edestaddrreq = -89;
	static constexpr Result emsgsize = -90;
	static constexpr Result eprototype = -91;
	static constexpr Result enoprotoopt = -92;
	static constexpr Result eprotonosupport = -93;
	static constexpr Result esocktnosupport = -94;
	static constexpr Result eopnotsupp = -95;
	static constexpr Result enotsup = eopnotsupp;
	static constexpr Result epfnosupport = -96;
	static constexpr Result eafnosupport = -97;
	static constexpr Result eaddrinuse = -98;
	static constexpr Result eaddrnotavail = -99;
	static constexpr Result enetdown = -100;
	static constexpr Result enetunreach = -101;
	static constexpr Result enetreset = -102;
	static constexpr Result econnaborted = -103;
	static constexpr Result econnreset = -104;
	static constexpr Result enobufs = -105;
	static constexpr Result eisconn = -106;
	static constexpr Result enotconn = -107;
	static constexpr Result eshutdown = -108;
	static constexpr Result etoomanyrefs = -109;
	static constexpr Result etimedout = -110;
	static constexpr Result econnrefused = -111;
	static constexpr Result ehostdown = -112;
	static constexpr Result ehostunreach = -113;
	static constexpr Result ealready = -114;
	static constexpr Result einprogress = -115;
	static constexpr Result estale = -116;
	static constexpr Result euclean = -117;
	static constexpr Result enotnam = -118;
	static constexpr Result enavail = -119;
	static constexpr Result eisnam = -120;
	static constexpr Result eremoteio = -121;
	static constexpr Result edquot = -122;
	static constexpr Result enomedium = -123;
	static constexpr Result emediumtype = -124;
	static constexpr Result ecanceled = -125;
	static constexpr Result enokey = -126;
	static constexpr Result ekeyexpired = -127;
	static constexpr Result ekeyrevoked = -128;
	static constexpr Result ekeyrejected = -129;
	static constexpr Result eownerdead = -130;
	static constexpr Result enotrecoverable = -131;
	static constexpr Result erfkill = -132;
	static constexpr Result ehwpoison = -133;
}}}
