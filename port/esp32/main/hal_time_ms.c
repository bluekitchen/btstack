#include "hal_time_ms.h"

uint32_t esp_log_timestamp();

uint32_t hal_time_ms(void) {
	// super hacky way to get ms
	return esp_log_timestamp();
}
