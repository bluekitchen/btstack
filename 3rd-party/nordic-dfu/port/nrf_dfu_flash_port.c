#include "nrf_dfu_types.h"
#include "nrf_log.h"
#include "stm32f4xx_hal.h"
#include "nrf_dfu_flash.h"

/* Base address of the Flash sectors */
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base address of Sector 0, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base address of Sector 1, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base address of Sector 2, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base address of Sector 3, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base address of Sector 4, 64 Kbytes   */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base address of Sector 5, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base address of Sector 6, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base address of Sector 7, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base address of Sector 8, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base address of Sector 9, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base address of Sector 10, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base address of Sector 11, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_END   (ADDR_FLASH_SECTOR_11 + (128 * 1024))

#define NRF_DFU_FLASH_VALIDATION    1

static uint32_t hal_get_sector_from_address(uint32_t addr)
{
    uint32_t sector = 0xFF;

    if ((addr < ADDR_FLASH_SECTOR_1) && (addr >= ADDR_FLASH_SECTOR_0)) {
        sector = FLASH_SECTOR_0;
    } else if((addr < ADDR_FLASH_SECTOR_2) && (addr >= ADDR_FLASH_SECTOR_1)) {
        sector = FLASH_SECTOR_1;
    } else if ((addr < ADDR_FLASH_SECTOR_3) && (addr >= ADDR_FLASH_SECTOR_2)) {
        sector = FLASH_SECTOR_2;
    } else if ((addr < ADDR_FLASH_SECTOR_4) && (addr >= ADDR_FLASH_SECTOR_3)) {
        sector = FLASH_SECTOR_3;
    } else if ((addr < ADDR_FLASH_SECTOR_5) && (addr >= ADDR_FLASH_SECTOR_4)) {
        sector = FLASH_SECTOR_4;
    } else if ((addr < ADDR_FLASH_SECTOR_6) && (addr >= ADDR_FLASH_SECTOR_5)) {
        sector = FLASH_SECTOR_5;
    } else if ((addr < ADDR_FLASH_SECTOR_7) && (addr >= ADDR_FLASH_SECTOR_6)) {
        sector = FLASH_SECTOR_6;
    } else if ((addr < ADDR_FLASH_SECTOR_8) && (addr >= ADDR_FLASH_SECTOR_7)) { 
        sector = FLASH_SECTOR_7;
    } else if ((addr < ADDR_FLASH_SECTOR_9) && (addr >= ADDR_FLASH_SECTOR_8)) { 
        sector = FLASH_SECTOR_8;
    } else if ((addr < ADDR_FLASH_SECTOR_10) && (addr >= ADDR_FLASH_SECTOR_9)) { 
        sector = FLASH_SECTOR_9;
    } else if ((addr < ADDR_FLASH_SECTOR_11) && (addr >= ADDR_FLASH_SECTOR_10)) { 
        sector = FLASH_SECTOR_10;
    } else if ((addr < ADDR_FLASH_SECTOR_END) && (addr >= ADDR_FLASH_SECTOR_11)) {
        sector = FLASH_SECTOR_11;
    }
    return sector;
}

static void hal_flash_read(void *dst, uint32_t addr, uint32_t len)
{
    memcpy(dst, (void *)((uint32_t *)addr), len);
}

static ret_code_t hal_flash_write(void *src, uint32_t addr, uint32_t len)
{
	unsigned int i;
	HAL_FLASH_Unlock();
	for (i = 0;i < len;i+=1){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, *(uint8_t *)(src+i));
	}

	HAL_FLASH_Lock();
#if NRF_DFU_FLASH_VALIDATION
    if (memcmp((void *)addr, src, len) != 0) {
        NRF_LOG_ERROR("Program Failed");
        return NRF_ERROR_INTERNAL;
    }
#endif
    return NRF_SUCCESS;
}

static ret_code_t hal_flash_erase(uint32_t addr, uint32_t len)
{
    uint32_t sector;
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError;

    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.NbSectors = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_1;	// safe value

    if ((addr + len) > (NRF_DFU_BANK1_START_ADDR + NRF_DFU_BANK1_SIZE)) {
        NRF_LOG_ERROR("out of boundary, start_addr:0x%x, len:0x%x", addr, len);
        return NRF_ERROR_INVALID_PARAM;
    }

    if (addr % NRF_DFU_FLASH_SECTOR_SIZE && len < NRF_DFU_FLASH_SECTOR_SIZE) {
        NRF_LOG_WARNING("start addr(0x%x) not alignd, and erase len is samller than one sector size(0x%x)",
                    addr, NRF_DFU_FLASH_SECTOR_SIZE);
        //which means this scope has been erased before, so just return success.
        return NRF_SUCCESS;
    }
    if (len < NRF_DFU_FLASH_SECTOR_SIZE) {
        len = NRF_DFU_FLASH_SECTOR_SIZE;
    }
    //lint -save -e611 (Suspicious cast)
    for (uint32_t i = 0; i < (len / NRF_DFU_FLASH_SECTOR_SIZE); i++) {
        sector = hal_get_sector_from_address(addr + i * NRF_DFU_FLASH_SECTOR_SIZE);
        if (sector > FLASH_SECTOR_11) {
            NRF_LOG_ERROR("get sector fail by invalid addr 0x%x ", addr + i * NRF_DFU_FLASH_SECTOR_SIZE);
            return NRF_ERROR_INTERNAL;
        }
        eraseInit.Sector = sector;
        HAL_FLASH_Unlock();
        HAL_FLASHEx_Erase(&eraseInit, &sectorError);
        HAL_FLASH_Lock();
    }

#if NRF_DFU_FLASH_VALIDATION
    for (size_t i = 0; i < len; i++) {
        uint8_t *val = (void *)(addr + i);
        if (*val != 0xff) {
            NRF_LOG_ERROR("Erase at 0x%x Failed", (int)val);
            return NRF_ERROR_INTERNAL;
        }
    }
#endif
    return NRF_SUCCESS;
}

static nrf_dfu_flash_hal_t flash_interface = {
    .read = hal_flash_read,
    .write = hal_flash_write,
    .erase = hal_flash_erase
};

nrf_dfu_flash_hal_t *nrf_dfu_flash_port_get_interface(void)
{
    return &flash_interface;
}