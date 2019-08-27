#!/bin/sh
grep goto -n -r ../../src

# For 9d49ac0b2

# ../../src/classic/rfcomm.c:2351:            goto fail;
# ../../src/classic/rfcomm.c:2363:        goto fail;
# ../../src/classic/rfcomm.c:2370:        goto fail;
# ../../src/classic/rfcomm.c:2406:        if (status) goto fail;

# rfcomm.c : 4 gotos in rfcomm_channel_create_internal
