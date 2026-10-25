#ifndef LHDCV5_MSVC_COMPAT_H
#define LHDCV5_MSVC_COMPAT_H

#ifdef _MSC_VER
#include <intrin.h>

#ifndef __attribute__
#define __attribute__(x)
#endif

static __forceinline int lhdcv5_msvc_clz(unsigned int value) {
    unsigned long index;
    if (_BitScanReverse(&index, value)) {
        return 31 - (int)index;
    }
    return 32;
}

#ifndef __builtin_clz
#define __builtin_clz(x) lhdcv5_msvc_clz((unsigned int)(x))
#endif
#endif

#endif