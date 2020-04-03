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

#ifndef R_SCI_UART_PRIVATE_API_H
#define R_SCI_UART_PRIVATE_API_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Private Instance API Functions. DO NOT USE! Use functions through Interface API structure instead.
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartOpen       (uart_ctrl_t * const p_ctrl, uart_cfg_t const * const p_cfg);
ssp_err_t R_SCI_UartRead       (uart_ctrl_t * const p_ctrl, uint8_t const * const p_dest, uint32_t const bytes);
ssp_err_t R_SCI_UartWrite      (uart_ctrl_t * const p_ctrl, uint8_t const * const p_src, uint32_t const bytes);
ssp_err_t R_SCI_UartBaudSet    (uart_ctrl_t * const p_ctrl, uint32_t const baudrate);
ssp_err_t R_SCI_UartInfoGet    (uart_ctrl_t * const p_ctrl, uart_info_t * const p_info);
ssp_err_t R_SCI_UartClose      (uart_ctrl_t * const p_ctrl);
ssp_err_t R_SCI_UartVersionGet (ssp_version_t * p_version);
ssp_err_t R_SCI_UartAbort      (uart_ctrl_t * const p_ctrl, uart_dir_t communication_to_abort);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_SCI_UART_PRIVATE_API_H */
