
typedef int EventGroupHandle_t;
typedef int EventBits_t;
typedef int BaseType_t;
typedef int TickType_t;

EventGroupHandle_t xEventGroupCreate( void );

EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToSet );

BaseType_t xEventGroupSetBitsFromISR(
                          EventGroupHandle_t xEventGroup,
                          const EventBits_t uxBitsToSet,
                          BaseType_t *pxHigherPriorityTaskWoken );

EventBits_t xEventGroupWaitBits(
                       const EventGroupHandle_t xEventGroup,
                       const EventBits_t uxBitsToWaitFor,
                       const BaseType_t xClearOnExit,
                       const BaseType_t xWaitForAllBits,
                       TickType_t xTicksToWait );
