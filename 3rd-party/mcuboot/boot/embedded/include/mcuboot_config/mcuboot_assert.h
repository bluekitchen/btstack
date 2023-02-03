
#include "port/boot_assert_port.h"

#ifdef assert
#undef assert
#endif
#define assert(arg)                                                   \
    do {                                                              \
        if (!(arg)) {                                                 \
            boot_port_assert_handler( __FILE__, __LINE__, __func__ ); \
        }                                                             \
    } while(0)
