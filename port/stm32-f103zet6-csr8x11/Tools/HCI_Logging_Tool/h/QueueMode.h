#pragma once

#ifdef	__cplusplus
enum EQueueMode
#else
typedef enum
#endif
{
	  QUEUE_MODE_DISCARD_QUEUE_CONTENTS_UPON_OVERFLOW  // when there's not enough room in the queue to add the next entry, discard the entire contents of the queue and add the next entry.

	, QUEUE_MODE_RETRY_UNTIL_ENQUEUE_SUCCESSFUL  // when there's not enough room in the queue to add the next entry, retry until successful.  Used for any device with onboard storage.

	, QUEUE_MODE_USE_SHARED_FILE  // uses shared file instead of shared memory.  Permanently disables enqueueing upon overflow (each subsequent attempted enqueue returns failure status).
}
#ifdef	__cplusplus
 ;
#else
 EQueueMode;
#endif
