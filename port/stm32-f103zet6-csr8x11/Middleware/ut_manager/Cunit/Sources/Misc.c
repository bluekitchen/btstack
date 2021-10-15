#include "time.h"
#include "bt_os_layer_api.h"

time_t time(time_t * timer)
{
	if (bt_os_layer_is_os_scheduler_start()) {
		*timer = bt_os_layer_get_system_tick();
		return bt_os_layer_get_system_tick();
	}
	return 0;
}

void exit(int status)
{
	status = status;
}

clock_t clock(void)
{
	if (bt_os_layer_is_os_scheduler_start()) {
		return bt_os_layer_get_system_tick();
	}
	return 0;
}
