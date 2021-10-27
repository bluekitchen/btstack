#include "includes.h"

#ifdef FREERTOS_ENABLE
static void AppTaskCreate (void);
static TaskHandle_t vHandleTaskATCommand = NULL;
static TaskHandle_t xHandleTaskBT = NULL;
#endif

int main(void)
{

	__set_PRIMASK(1);  
	
	bsp_Init(); 

#ifdef AT_CMD_ENABLE	
	at_command_init();
#endif

#ifdef CUNIT_ENABLE
	cunit_init();
#endif

#ifdef FREERTOS_ENABLE
	AppTaskCreate();

    vTaskStartScheduler();
#endif
}

#ifdef FREERTOS_ENABLE

void vBT_Task(void *pvParameters)
{
    uint32_t i = 0;
    printf("this is bt task! \r\n");
	while(1)
    {
        printf("this is bt task! i = %d! \r\n", i);
        i++;
    }
}

static void AppTaskCreate (void)
{
    xTaskCreate( vTaskATCommand,
                 "vTaskATCommand",     	
                 512,               	
                 NULL,              	
                 2,                 	
                 &vHandleTaskATCommand );  
	
    xTaskCreate( vBT_Task,	      	    
                 "vBT_Task",     	    
                 512,               	
                 NULL,              	
                 4,                 	
                 &xHandleTaskBT );
}
#endif
