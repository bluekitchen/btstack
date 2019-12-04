# Modifications to Segger RTT v6.54 for BTstack
- SEGGER_RTT_GCC.c:
  - include <btstack_config.h> 
  - check for ENABLE_SEGGER_RTT
  - fix prototype for `_write_r`:
    - old: `int _write_r(struct _reent *r, int file, const void *ptr, int len);`
    - new: `_ssize_t _write_r(struct _reent *r, int file, const void *ptr, size_t len) 
- SEGGER_RTT.h/c
  - add SEGGER_RTT_GetAvailWriteSpace
