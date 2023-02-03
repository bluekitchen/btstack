
#ifndef _FLASH_MAP_BACKEND_H_
#define _FLASH_MAP_BACKEND_H_

/**
 *
 * Provides abstraction of flash regions for type of use.
 * I.e. dude where's my image?
 *
 * System will contain a map which contains flash areas. Every
 * region will contain flash identifier, offset within flash and length.
 *
 * 1. This system map could be in a file within filesystem (Initializer
 * must know/figure out where the filesystem is at).
 * 2. Map could be at fixed location for project (compiled to code)
 * 3. Map could be at specific place in flash (put in place at mfg time).
 *
 * Note that the map you use must be valid for BSP it's for,
 * match the linker scripts when platform executes from flash,
 * and match the target offset specified in download script.
 */
#include <inttypes.h>

/**
 * @brief Structure describing an area on a flash device.
 *
 * Multiple flash devices may be available in the system, each of
 * which may have its own areas. For this reason, flash areas track
 * which flash device they are part of.
 */
struct flash_area {
	/**
	 * This flash area's ID; unique in the system.
	 */
	uint8_t fa_id;

	/**
	 * ID of the flash device this area is a part of.
	 */
	uint8_t fa_device_id;

	uint16_t pad16;

	/**
	 * This area's offset, relative to the beginning of its flash
	 * device's storage.
	 */
	uint32_t fa_off;

	/**
	 * This area's size, in bytes.
	 */
	uint32_t fa_size;
};

/**
 * @brief Structure describing a sector within a flash area.
 *
 * Each sector has an offset relative to the start of its flash area
 * (NOT relative to the start of its flash device), and a size. A
 * flash area may contain sectors with different sizes.
 */
struct flash_sector {
	/**
	 * Offset of this sector, from the start of its flash area (not device).
	 */
	uint32_t fs_off;

	/**
	 * Size of this sector, in bytes.
	 */
	uint32_t fs_size;
};

/*< Opens the area for use. id is one of the `fa_id`s */
int flash_area_open(uint8_t id, const struct flash_area **);


void flash_area_close(const struct flash_area *);


/*< Reads `len` bytes of flash memory at `off` to the buffer at `dst` */
int flash_area_read(const struct flash_area *, uint32_t off, void *dst,
					 uint32_t len);

/*< Writes `len` bytes of flash memory at `off` from the buffer at `src` */
int flash_area_write(const struct flash_area *, uint32_t off,
					 const void *src, uint32_t len);

/*< Erases `len` bytes of flash memory at `off` */
int flash_area_erase(const struct flash_area *, uint32_t off, uint32_t len);

/*< Returns this `flash_area`s alignment */
size_t flash_area_align(const struct flash_area *);

/*< What is value is read from erased flash bytes. */
uint8_t flash_area_erased_val(const struct flash_area *);

/*< Given flash area ID, return info about sectors within the area. */
int flash_area_get_sectors(int fa_id, uint32_t *count,
					 struct flash_sector *sectors);

/*< Returns the `fa_id` for slot, where slot is 0 (primary) or 1 (secondary).
	`image_index` (0 or 1) is the index of the image. Image index is
	relevant only when multi-image support support is enabled */
int	 flash_area_id_from_multi_image_slot(int image_index, int slot);

/*< Returns the slot (0 for primary or 1 for secondary), for the supplied
	`image_index` and `area_id`. `area_id` is unique and is represented by
	`fa_id` in the `flash_area` struct. */
int	 flash_area_id_to_multi_image_slot(int image_index, int area_id);

int flash_area_id_from_image_slot(int slot);

static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
	return (uint32_t)fa->fa_off;
}

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
	return (uint32_t)fa->fa_size;
}

static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
	return fa->fa_id;
}

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
	return fa->fa_device_id;
}

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
	return fs->fs_off;
}

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
	return fs->fs_size;
}

#endif
