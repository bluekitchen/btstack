#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#include "btstack_bool.h"

// redirect btstack_assert to test_assert
#define HAVE_ASSERT
#define btstack_assert test_assert

#if defined __cplusplus
extern "C" {
#endif
extern void test_assert(bool condition);
#if defined __cplusplus
}
#endif

#define ENABLE_PRINTF_HEXDUMP

#endif