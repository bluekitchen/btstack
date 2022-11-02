/**
 * Copyright (c) 2016 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**@file
 *
 * @defgroup sdk_nrf_dfu_utils DFU utilities
 * @{
 * @ingroup  nrf_dfu
 */

#ifndef NRF_DFU_UTILS_H__
#define NRF_DFU_UTILS_H__

#include <stdint.h>
#include <stdbool.h>
#include "nrf_dfu_types.h"
//#include "app_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@brief Macro for increasing a number to the nearest (larger) multiple of another number.
 *
 * @param[in]  alignment  The number to align to.
 * @param[in]  number     The number to align (increase).
 *
 * @return The aligned (increased) @p number.
 */
#define ALIGN_NUM(alignment, number) (((number) - 1) + (alignment) - (((number) - 1) % (alignment)))

/**@brief Function for encoding a uint16 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint16_encode(uint16_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x00FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0xFF00) >> 8);
    return sizeof(uint16_t);
}

/**@brief Function for encoding a three-byte value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint24_encode(uint32_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x000000FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0x0000FF00) >> 8);
    p_encoded_data[2] = (uint8_t) ((value & 0x00FF0000) >> 16);
    return 3;
}

/**@brief Function for encoding a uint32 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint32_encode(uint32_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x000000FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0x0000FF00) >> 8);
    p_encoded_data[2] = (uint8_t) ((value & 0x00FF0000) >> 16);
    p_encoded_data[3] = (uint8_t) ((value & 0xFF000000) >> 24);
    return sizeof(uint32_t);
}

/**@brief Function for encoding a uint40 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint40_encode(uint64_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x00000000FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0x000000FF00) >> 8);
    p_encoded_data[2] = (uint8_t) ((value & 0x0000FF0000) >> 16);
    p_encoded_data[3] = (uint8_t) ((value & 0x00FF000000) >> 24);
    p_encoded_data[4] = (uint8_t) ((value & 0xFF00000000) >> 32);
    return 5;
}

/**@brief Function for encoding a uint48 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint48_encode(uint64_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x0000000000FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0x00000000FF00) >> 8);
    p_encoded_data[2] = (uint8_t) ((value & 0x000000FF0000) >> 16);
    p_encoded_data[3] = (uint8_t) ((value & 0x0000FF000000) >> 24);
    p_encoded_data[4] = (uint8_t) ((value & 0x00FF00000000) >> 32);
    p_encoded_data[5] = (uint8_t) ((value & 0xFF0000000000) >> 40);
    return 6;
}

/**@brief Function for decoding a uint16 value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value.
 */
static __inline uint16_t uint16_decode(const uint8_t * p_encoded_data)
{
        return ( (((uint16_t)((uint8_t *)p_encoded_data)[0])) |
                 (((uint16_t)((uint8_t *)p_encoded_data)[1]) << 8 ));
}

/**@brief Function for decoding a uint16 value in big-endian format.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value.
 */
static __inline uint16_t uint16_big_decode(const uint8_t * p_encoded_data)
{
        return ( (((uint16_t)((uint8_t *)p_encoded_data)[0]) << 8 ) |
                 (((uint16_t)((uint8_t *)p_encoded_data)[1])) );
}

/**@brief Function for decoding a three-byte value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value (uint32_t).
 */
static __inline uint32_t uint24_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 16));
}

/**@brief Function for decoding a uint32 value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value.
 */
static __inline uint32_t uint32_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 16) |
             (((uint32_t)((uint8_t *)p_encoded_data)[3]) << 24 ));
}

/**@brief Function for decoding a uint32 value in big-endian format.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value.
 */
static __inline uint32_t uint32_big_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 24) |
             (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 16) |
             (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 8)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[3]) << 0) );
}

/**
 * @brief Function for encoding an uint16 value in big-endian format.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data will be written.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint16_big_encode(uint16_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) (value >> 8);
    p_encoded_data[1] = (uint8_t) (value & 0xFF);

    return sizeof(uint16_t);
}

/*lint -esym(526, __rev) */
/*lint -esym(628, __rev) */
/**@brief Function for encoding a uint32 value in big-endian format.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data will be written.
 *                               The address pointed to must be word alligned.
 *
 * @return      Number of bytes written.
 */
static __inline uint8_t uint32_big_encode(uint32_t value, uint8_t * p_encoded_data)
{
    *(uint32_t *)p_encoded_data = __REV(value);
    return sizeof(uint32_t);
}

/**@brief Function for decoding a uint40 value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value. (uint64_t)
 */
static __inline uint64_t uint40_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint64_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
             (((uint64_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
             (((uint64_t)((uint8_t *)p_encoded_data)[2]) << 16) |
             (((uint64_t)((uint8_t *)p_encoded_data)[3]) << 24) |
             (((uint64_t)((uint8_t *)p_encoded_data)[4]) << 32 ));
}

/**@brief Function for decoding a uint48 value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value. (uint64_t)
 */
static __inline uint64_t uint48_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint64_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
             (((uint64_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
             (((uint64_t)((uint8_t *)p_encoded_data)[2]) << 16) |
             (((uint64_t)((uint8_t *)p_encoded_data)[3]) << 24) |
             (((uint64_t)((uint8_t *)p_encoded_data)[4]) << 32) |
             (((uint64_t)((uint8_t *)p_encoded_data)[5]) << 40 ));
}

/**
 * Round up val to the next page boundary
 */
#define ALIGN_TO_PAGE(val) ALIGN_NUM((CODE_PAGE_SIZE), (val))


/** @brief Function for getting the start address of bank 0.
 *
 * @note Bank 0 starts after the SoftDevice if a SoftDevice is present.
 *
 * @return The start address of bank 0.
 */
uint32_t nrf_dfu_bank0_start_addr(void);


/** @brief Function for getting the start address of bank 1.
 *
 * @return The start address of bank 1.
 */
uint32_t nrf_dfu_bank1_start_addr(void);


/** @brief Function for getting the start address of the app.
 *
 * @return  The start address of the bootable app.
 */
uint32_t nrf_dfu_app_start_address(void);


/** @brief Function for getting the start address of the SoftDevice.
 *
 * @return  The start address of the SoftDevivce.
 */
uint32_t nrf_dfu_softdevice_start_address(void);


/** @brief Function for finding and preparing a place in flash in which to store a DFU update.
 *
 * @details This function checks the size requirements and selects a location for
 *          placing the cache of the DFU images.
 *          The function tries to find enough space after the existing firmwares. If there is not
 *          enough space, the present application is deleted. If there is still not enough space,
 *          the SoftDevice is deleted.
 *          If @p single_bank is true, the default behavior is to immediately delete the app and
 *          SoftDevice as necessary to place the new firmware at its intended location. If the
 *          intended location cannot be made available, or if the update is a bootloader update,
 *          the update will be a dual bank update, and nothing will be deleted by this function
 *          except when needed for size.
 *          If @p keep_app is true, the app is never deleted by this function. Likewise if @p
 *          keep_softdevice is true, the SoftDevice is never deleted by this function.
 *          If the new firmware cannot fit within the constraints, nothing is deleted and the
 *          function fails.
 *
 * @param[in]   required_size   Requirements for the size of the new image.
 * @param[in]   single_bank     Whether to put the firmware directly where it's meant to go.
 *                              @p keep_app and @p keep_softdevice take precedence over this.
 * @param[in]   keep_app        True to ensure the app is not deleted by this function. This
 *                              effectively enforces dual bank update.
 * @param[out]  keep_softdevice True to ensure the SoftDevice is not deleted by this function.
 *
 * @retval      NRF_SUCCESS         If a cache location was found for the DFU process.
 * @retval      NRF_ERROR_NO_MEM    If there is not enough space available to receive the update.
 *                                  Nothing has been deleted.
 */
uint32_t nrf_dfu_cache_prepare(uint32_t required_size, bool single_bank, bool keep_app, bool keep_softdevice);


/**@brief Function for making sure a SoftDevice is not recognized as such anymore.
 *
 * @details It works by overwriting the magic number of the SoftDevice with 0s. The
 *          magic number is used throughout the bootloader to detect whether a SoftDevice
 *          is present.
 *
 * @warning This function should only be called when both banks are already invalid.
 *          because the (implicit) position of the banks will shift when the SoftDevice
 *          is invalidated.
 */
void nrf_dfu_softdevice_invalidate(void);


/**@brief Function for making sure a bank is not copied or booted.
 *
 * @details This also sets the size of the bank to 0.
 *
 * @param[in]  p_bank Pointer to the bank to be invalidated.
 */
void nrf_dfu_bank_invalidate(nrf_dfu_bank_t * const p_bank);

#ifdef __cplusplus
}
#endif

#endif // NRF_DFU_UTILS_H__

/** @} */
