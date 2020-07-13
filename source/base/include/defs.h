/** Main public header for SGFBASE. **/

#ifndef SGFBASE_DEFS_H_
#define SGFBASE_DEFS_H_

// figure out whether debug build -- CMAKE will set NDEBUG for a release build
#ifndef NDEBUG
	#define SGF_DEBUG
#else
	#undef SGF_DEBUG
#endif

// language includes
#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <float.h>

#include <fstream>
#include <sstream>
#include <iomanip>

#include <algorithm>
#include <set>
#include <vector>
#include <map>
#include <bitset>

#include <string>

// some typedefs
typedef int32_t int32, i32;
typedef uint32_t uint32, u32;
typedef int64_t int64, ll, i64;
typedef uint64_t uint64, ull, u64;
typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort, u16;
typedef unsigned char uchar, u8;

typedef unsigned char byte, *pbyte, *PBYTE;
typedef char CHAR, *pchar, *PCHAR;

typedef void VOID, *PVOID;


// some limits
#ifndef U8_MAX
	#define U8_MAX (UCHAR_MAX)
#endif
#ifndef U16_MAX
	#define U16_MAX (USHRT_MAX)
#endif
#ifndef S16_MAX
	#define S16_MAX (SHRT_MAX)
#endif

#define HEXADECIMAL_DIGITS_PER_BYTE 2

#define BITS_PER_BYTE (8)

#ifdef SGF_DEBUG // from http://stackoverflow.com/questions/652815/has-anyone-ever-had-a-use-for-the-counter-pre-processor-macro
	#define WAYPOINT \
    	do { printf("[DEBUG]: at marker %d\n", __COUNTER__); } while(0);
#else
	#define WAYPOINT
#endif

/**
 * (from http://stackoverflow.com/questions/807244/c-compiler-asserts-how-to-implement)
 *
 * A compile time assertion check.
 *
 *  Validate at compile time that the predicate is true without
 *  generating code. This can be used at any point in a source file
 *  where typedef is legal.
 *
 *  On success, compilation proceeds normally.
 *
 *  On failure, attempts to typedef an array type of negative size. The
 *  offending line will look like
 *      typedef assertion_failed_file_h_42[-1]
 *  where file is the content of the second parameter which should
 *  typically be related in some obvious way to the containing file
 *  name, 42 is the line number in the file on which the assertion
 *  appears, and -1 is the result of a calculation based on the
 *  predicate failing.
 *
 *  \param predicate The predicate to test. It must evaluate to
 *  something that can be coerced to a normal C boolean.
 *
 *  \param file A sequence of legal identifier characters that should
 *  uniquely identify the source file in which this condition appears.
 */
#define CTVERIFY(_predicate, _file) _impl_CTVERIFY_LINE(_predicate,__LINE__,_file)

#define _impl_PASTE(a,b) a##b
#define _impl_CTVERIFY_LINE(_predicate, _line, _file) \
    typedef char _impl_PASTE(assertion_failed_##_file##_,_line)[2*!!(_predicate)-1];


// memory management
#define Allocate malloc
#define Free free

#define ZeroMemory(_p, _s) memset((_p), 0, (_s))
#define CopyMemory memcpy

#define CompareMemory(_p1, _p2, _s) (memcmp((_p1), (_p2), (_s)) == 0 ? true : false)

#ifdef SGF_DEBUG
#define DebugZeroMemory ZeroMemory
#else
#define DebugZeroMemory
#endif

// misc. preprocessor macros
#define foreach(T, _c, i) \
	for(T::iterator i = (_c).begin(); i != (_c).end(); ++i)

#define foreach_const(T, _c, i) \
	for(T::const_iterator i = (_c).begin(); i != (_c).end(); ++i)

#define pair_foreach(T1, T2, _c, i) \
	for(T1, T2::iterator i = (_c).begin(); i != (_c).end(); ++i)

#define pair_foreach_const(T1, T2, _c, i) \
	for(T1, T2::const_iterator i = (_c).begin(); i != (_c).end(); ++i)

#define GET_INDEX(_i, _j, _numCol) ((_i) * (_numCol) + (_j))

#define MAP_LOOKUP(_m, _s) ((_m).find(_s))
#define MAP_CONTAINS(_m, _s) ((MAP_LOOKUP((_m), (_s)) == (_m).end()) ? false : true)

#define SET_CONTAINS MAP_CONTAINS

#define FILL_ARRAY(_v, _sz, _f) { for(u64 zvi=0; zvi<(_sz); zvi++) { (_v)[zvi] = (_f); } }
#define ZERO_VECTOR(_v, _sz) FILL_ARRAY(_v, _sz, 0)

#ifndef MIN
#define MIN(_a, _b) (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef MAX
#define MAX(_a, _b) (((_a) < (_b)) ? (_b) : (_a))
#endif

#define SIGN(_x) (((_x) < 0) ? -1 : +1)

#define SQRT_DBL_MIN (1.4916681462400413e-154)


#if _WIN32
    #define getcwd _getcwd
	#include <process.h>
	#define getpid _getpid
#else
	#include <unistd.h> // needed for getpid()
#endif // _WIN32

using namespace std;

// Libraries headers
#include <cmath>
#include <cerrno>

// Code headers
#include "easylogging++.h"
inline void __release_assert(const char* expr, const char* file, int line)
{
	fprintf(stderr, "Assertion '%s' failed, file '%s' line '%d'.", expr, file, line);
	LOG(ERROR) << "Assertion '" << expr << "' failed, file: '" << file << "', line: '" << line << "'.";
	abort(); exit(-1); // make sure we exit no matter what
}

// ---- assertions ------ //
#ifndef __USE_VERIFY
#define __USE_VERIFY
	#ifdef SGF_DEBUG
		#define ASSERT assert
	#else
		#define ASSERT(_a) { bool bAssert = _a; if(bAssert == false){ __release_assert(#_a, __FILE__, __LINE__); } }
	#endif

	#define VERIFY ASSERT

	#define CODING_ERROR ASSERT(0 == 1)
#endif
// ---- //


//---- timing stuff ----- //
static long __tic_get_usec(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000000L + t.tv_usec;
}

static double __toc_elapsed_secs(const long st)
{
	long diff = __tic_get_usec() - st;
	VERIFY(diff >= 0);
	return double(diff / 1000000.0);
}

#ifndef TIC
	#define TICV (__tic_get_usec())
	#define TIC(_v) long (_v) = TICV;
#endif
#ifndef TOC
	#define TOC(_v) (__toc_elapsed_secs((_v)))
#endif
// ---- //

// includes for base
#include "singleton.h"

#include "misc.h"

extern "C"
{
	#include "risaac.h"
}

#include "ini.h"
typedef INI<> ini_t; //typedef INI<string, string, string> ini_t;

#include "rng.h"
#include "params.h"

#include "mathutils.h"
#include "fileutils.h"

#include "rtm.h"

#include "config.h"

#include "store.h"

#include "outputter.h"
#include "genmodel.h"

#include "synth.h"

#endif /* SGFBASE_DEFS_H_ */
