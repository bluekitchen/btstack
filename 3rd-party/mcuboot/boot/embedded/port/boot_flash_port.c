#include <string.h>
#include <stdlib.h>

#include "mcuboot_config/mcuboot_logging.h"
#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"
#include "bootutil/bootutil.h"
#include "stm32f4xx_hal.h"

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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define BOOTLOADER_SIZE                     (64 * 1024 )
#define APPLICATION_SIZE                    (128 * 1024)
#define SCRATCH_SIZE                        (128 * 1024)

#define BOOTLOADER_START_ADDRESS            (ADDR_FLASH_SECTOR_0)
#define APPLICATION_PRIMARY_START_ADDRESS   (ADDR_FLASH_SECTOR_5)
#define APPLICATION_SECONDARY_START_ADDRESS (ADDR_FLASH_SECTOR_6)
#define SCRATCH_START_ADDRESS               (ADDR_FLASH_SECTOR_8)

#define BOOTLOADER_SECTOR_SIZE              (16 * 1024)
#define APPLICATION_SECTOR_SIZE             (128 * 1024)
#define SCRATCH_SECTOR_SIZE                 (128 * 1024)

#define VALIDATE_PROGRAM_OP                 1

static const struct flash_area bootloader = {
    .fa_id = FLASH_AREA_BOOTLOADER,
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = BOOTLOADER_START_ADDRESS,
    .fa_size = BOOTLOADER_SIZE,
};

static const struct flash_area primary_img0 = {
    .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = APPLICATION_PRIMARY_START_ADDRESS,
    .fa_size = APPLICATION_SIZE,
};

static const struct flash_area secondary_img0 = {
    .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = APPLICATION_SECONDARY_START_ADDRESS,
    .fa_size = APPLICATION_SIZE,
};

static const struct flash_area scratch_img0 = {
    .fa_id = FLASH_AREA_IMAGE_SCRATCH,
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = SCRATCH_START_ADDRESS,
    .fa_size = SCRATCH_SIZE,
};

static const struct flash_area *s_flash_areas[] = {
    &bootloader,
    &primary_img0,
    &secondary_img0,
    &scratch_img0,
};

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

static int hal_flash_write(void *src, uint32_t addr, uint32_t len)
{
	unsigned int i;
	HAL_FLASH_Unlock();
	for (i = 0;i < len;i+=1){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, *(uint8_t *)(src+i));
	}

	HAL_FLASH_Lock();
#if VALIDATE_PROGRAM_OP
    if (memcmp((void *)addr, src, len) != 0) {
        MCUBOOT_LOG_ERR("Program Failed");
        assert(0);
        return -1;
    }
#endif
    return 0;
}

static int hal_flash_erase(uint32_t addr, uint32_t len)
{
    uint32_t sector;
	FLASH_EraseInitTypeDef eraseInit;

    sector = hal_get_sector_from_address(addr);
    if (sector > FLASH_SECTOR_11) {
        MCUBOOT_LOG_ERR("get sector fail by invalid addr 0x%x ", addr);
        return -1;
    }
	eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	eraseInit.Sector = sector;
	eraseInit.NbSectors = 1;
	eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_1;	// safe value
	uint32_t sectorError;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&eraseInit, &sectorError);
	HAL_FLASH_Lock();

#if VALIDATE_PROGRAM_OP
    for (size_t i = 0; i < len; i++) {
        uint8_t *val = (void *)(addr + i);
        if (*val != 0xff) {
            MCUBOOT_LOG_ERR("Erase at 0x%x Failed", (int)val);
            assert(0);
            return -1;
        }
    }
#endif
    return 0;
}

static const struct flash_area *prv_lookup_flash_area(uint8_t id) {
    for (size_t i = 0; i < ARRAY_SIZE(s_flash_areas); i++) {
        const struct flash_area *area = s_flash_areas[i];
        if (id == area->fa_id) {
            return area;
        }
    }
    return NULL;
}

int flash_area_open(uint8_t id, const struct flash_area **area_outp)
{
    MCUBOOT_LOG_DBG("ID=%d", (int)id);
    const struct flash_area *area = prv_lookup_flash_area(id);
    *area_outp = area;
    return area != NULL ? 0 : -1;
}

void flash_area_close(const struct flash_area *area)
{
    // no cleanup to do for this flash part
}

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
                    uint32_t len)
{
    uint32_t read_len = 0;
    uint32_t read_addr = fa->fa_off + off;
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    const uint32_t end_offset = off + len;

    if (end_offset > fa->fa_size) {
        MCUBOOT_LOG_ERR("Out of Bounds (0x%x vs 0x%x)", end_offset, fa->fa_size);
        return -1;
    }

    if ( dst != NULL &&  len > 0) {
        hal_flash_read(dst, read_addr, len);
    }

    return 0;
}

int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src,
                     uint32_t len)
{
    uint32_t write_len = len, write_data = 0;
    void *write_ptr = (void *)src;
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    const uint32_t end_offset = off + len;

    if (end_offset > fa->fa_size) {
        MCUBOOT_LOG_ERR("Out of Bounds (0x%x vs 0x%x)", end_offset, fa->fa_size);
        return -1;
    }

    const uint32_t start_addr = fa->fa_off + off;
    MCUBOOT_LOG_DBG("Addr: 0x%08x Length: 0x%x", (int)start_addr, (int)len);
    hal_flash_write(src, start_addr, len);

    return 0;
}

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    uint32_t sector_size;
    int ret;

    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    if (fa->fa_id == FLASH_AREA_BOOTLOADER) {
        if ((len % BOOTLOADER_SECTOR_SIZE) != 0 || (off % BOOTLOADER_SECTOR_SIZE) != 0) {
            MCUBOOT_LOG_ERR("Not aligned on sector Offset: 0x%x Length: 0x%x", (int)off, (int)len);
            return -1;
        }
        sector_size = BOOTLOADER_SECTOR_SIZE;
    } else if (fa->fa_id == FLASH_AREA_IMAGE_PRIMARY(0)
        || fa->fa_id == FLASH_AREA_IMAGE_SECONDARY(0)) {
        if ((len % APPLICATION_SECTOR_SIZE) != 0 || (off % APPLICATION_SECTOR_SIZE) != 0) {
            MCUBOOT_LOG_ERR("Not aligned on sector Offset: 0x%x Length: 0x%x", (int)off, (int)len);
            return -1;
        }
        sector_size = APPLICATION_SECTOR_SIZE;
    } else if (fa->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        if ((len % SCRATCH_SECTOR_SIZE) != 0 || (off % SCRATCH_SECTOR_SIZE) != 0) {
            MCUBOOT_LOG_ERR("Not aligned on sector Offset: 0x%x Length: 0x%x", (int)off, (int)len);
            return -1;
        }
        sector_size = SCRATCH_SECTOR_SIZE;
    }

    const uint32_t start_addr = fa->fa_off + off;
    MCUBOOT_LOG_DBG("%s: Addr: 0x%08x Length: 0x%x", __func__, (int)start_addr, (int)len);

    for (uint32_t i = 0; i < (len / sector_size); i++) {
        ret = hal_flash_erase(start_addr + i * sector_size, sector_size);
        if (ret != 0) {
            MCUBOOT_LOG_ERR("Erase fail at addr: 0x%x ", start_addr + sector_size * i);
            return -1;
        }
    }
    return 0;
}

size_t flash_area_align(const struct flash_area *area)
{
    return 4;
}

uint8_t flash_area_erased_val(const struct flash_area *area)
{
    return 0xff;
}

/* look through all the sectors that the slot occupied */
int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors)
{
    const struct flash_area *fa = prv_lookup_flash_area(fa_id);
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    size_t sector_size = 0;
    uint32_t total_count = 0;

    if (fa_id == FLASH_AREA_BOOTLOADER)
        sector_size = BOOTLOADER_SECTOR_SIZE;
    else if (fa_id == FLASH_AREA_IMAGE_PRIMARY(0) || fa_id == FLASH_AREA_IMAGE_SECONDARY(0))
        sector_size = APPLICATION_SECTOR_SIZE;
    else if (fa_id == FLASH_AREA_IMAGE_SCRATCH)
        sector_size = SCRATCH_SECTOR_SIZE;

    for (size_t off = 0; off < fa->fa_size; off += sector_size) {
        // Note: Offset here is relative to flash area, not device
        sectors[total_count].fs_off = off;
        sectors[total_count].fs_size = sector_size;
        total_count++;
    }

    *count = total_count;
    return 0;
}

/* MCUboot needs to be able to resolve the “flash area id” to read from 
a given image index and slot id. The “slot” argument is set to 0 when 
looking up the primary slot and 1 to lookup the secondary slot.*/
int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    MCUBOOT_LOG_DBG("img_index:%d, slot:%d", image_index, slot);
    switch (slot) {
      case 0:
        return FLASH_AREA_IMAGE_PRIMARY(image_index);
      case 1:
        return FLASH_AREA_IMAGE_SECONDARY(image_index);
    }

    MCUBOOT_LOG_ERR("Unexpected Request: image_index=%d, slot=%d", image_index, slot);
    return -1; /* flash_area_open will fail on that */
}

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa)
{
    return -1;
}

void mcuboot_assert_handler(const char *file, int line, const char *func)
{
    printf("assertion failed: file \"%s\", line %d, func: %s\n", file, line, func);
    abort();
}
