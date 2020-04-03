/***********************************************************************************************************************
 * Copyright [2015-2017] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
 * 
 * This file is part of Renesas SynergyTM Software Package (SSP)
 *
 * The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
 * and/or its licensors ("Renesas") and subject to statutory and contractual protections.
 *
 * This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
 * Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
 * or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
 * MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
 * "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
 * CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
 * CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
 * included in this file may be subject to different terms.
 **********************************************************************************************************************/

#ifndef R_FLASH_LP_PRIVATE_API_H
#define R_FLASH_LP_PRIVATE_API_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Private Instance API Functions. DO NOT USE! Use functions through Interface API structure instead.
 **********************************************************************************************************************/
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t R_FLASH_LP_Open(flash_ctrl_t * const p_ctrl, flash_cfg_t  const * const p_cfg) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_Write(flash_ctrl_t * const p_ctrl, uint32_t const src_address, uint32_t flash_address, uint32_t const num_bytes) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_Read(flash_ctrl_t * const p_ctrl, uint8_t * p_dest_address, uint32_t const flash_address, uint32_t const num_bytes)PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_Erase(flash_ctrl_t * const p_ctrl, uint32_t const address, uint32_t const num_blocks) PLACE_IN_RAM_SECTION;
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t R_FLASH_LP_BlankCheck(flash_ctrl_t * const p_ctrl, uint32_t const address, uint32_t num_bytes, flash_result_t *blank_check_result) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_Close(flash_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_StatusGet(flash_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_AccessWindowSet(flash_ctrl_t * const p_ctrl, uint32_t const start_addr, uint32_t const end_addr) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_AccessWindowClear(flash_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_IdCodeSet(flash_ctrl_t * const p_ctrl, uint8_t const * const p_id_code, flash_id_code_mode_t mode) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_Reset(flash_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_StartUpAreaSelect(flash_ctrl_t * const p_ctrl, flash_startup_area_swap_t swap_type, bool is_temporary) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_UpdateFlashClockFreq (flash_ctrl_t * const  p_ctrl) PLACE_IN_RAM_SECTION;
ssp_err_t R_FLASH_LP_VersionGet (ssp_version_t * const p_version);
ssp_err_t R_FLASH_LP_InfoGet (flash_ctrl_t * const p_ctrl, flash_info_t  * const p_info);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_FLASH_LP_PRIVATE_API_H */
