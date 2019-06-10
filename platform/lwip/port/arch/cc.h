#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

/* see https://sourceforge.net/p/predef/wiki/OperatingSystems/ */
#if defined __ANDROID__
#define LWIP_UNIX_ANDROID
#elif defined __linux__
#define LWIP_UNIX_LINUX
#elif defined __APPLE__
#define LWIP_UNIX_MACH
#elif defined __OpenBSD__
#define LWIP_UNIX_OPENBSD
#elif defined __CYGWIN__
#define LWIP_UNIX_CYGWIN
#elif defined __GNU__
#define LWIP_UNIX_HURD
#endif

#if defined(LWIP_UNIX_MACH)
/* sys/types.h and signal.h bring in Darwin byte order macros. pull the
   header here and disable LwIP's version so that apps still can get
   the macros via LwIP headers and use system headers */
#include <sys/types.h>
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

#define lwip_htons(x) htons(x)
#define lwip_htonl(x) htonl(x)
#endif


#endif
