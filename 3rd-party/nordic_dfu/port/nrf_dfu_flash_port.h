#ifndef NRF_DFU_FLASH_PORT_H__
#define NRF_DFU_FLASH_PORT_H__

#include <stdint.h>
#include <stdbool.h>
#include "nrf_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

nrf_dfu_flash_hal_t *nrf_dfu_flash_port_get_interface(void);

#ifdef __cplusplus
}
#endif


#endif // NRF_DFU_FLASH_PORT_H__
/** @} */
