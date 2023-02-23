# Modifications to Segger RTT v6.54 for BTstack
- SEGGER_RTT_GCC.c:
  - include <btstack_config.h> 
  - check for ENABLE_SEGGER_RTT
- SEGGER_RTT.h:
  - include SEGGER_RTT_Conf.h from current folder
