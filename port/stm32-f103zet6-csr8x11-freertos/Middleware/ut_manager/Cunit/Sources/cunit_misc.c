#include "time.h"
#include "FreeRTOS.h"
#include "task.h"

time_t time(time_t * timer)
{
	if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState()) {
		*timer = xTaskGetTickCount();
		return xTaskGetTickCount();
	}
	return 0;
}

void exit(int status)
{
	status = status;
}

clock_t clock(void)
{
	if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState()) {
		return xTaskGetTickCount();
	}
	return 0;
}
