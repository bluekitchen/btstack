/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"

#include "btstack_run_loop.h"

u32_t
sys_now(void){
  return btstack_run_loop_get_time_ms();
}

#if 0
void
sys_init(void)
{
}
u32_t
sys_jiffies(void){
}
#endif
