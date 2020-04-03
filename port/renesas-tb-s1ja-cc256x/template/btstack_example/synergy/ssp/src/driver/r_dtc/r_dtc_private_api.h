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

#ifndef R_DTC_R_DTC_PRIVATE_API_H_
#define R_DTC_R_DTC_PRIVATE_API_H_

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Private Instance API Functions. DO NOT USE! Use functions through Interface API structure instead.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Open (transfer_ctrl_t        * const p_ctrl,
                      transfer_cfg_t const * const   p_cfg);
ssp_err_t R_DTC_Reset (transfer_ctrl_t        * const    p_ctrl,
                       void const * volatile             p_src,
                       void                   * volatile p_dest,
                       uint16_t const                    num_transfers);
ssp_err_t R_DTC_Start     (transfer_ctrl_t        * const p_ctrl,
                           transfer_start_mode_t          mode);
ssp_err_t R_DTC_Stop      (transfer_ctrl_t        * const p_ctrl);
ssp_err_t R_DTC_Enable    (transfer_ctrl_t        * const p_ctrl);
ssp_err_t R_DTC_Disable   (transfer_ctrl_t        * const p_ctrl);
ssp_err_t R_DTC_InfoGet   (transfer_ctrl_t        * const p_ctrl,
                           transfer_properties_t  * const p_info);
ssp_err_t R_DTC_Close (transfer_ctrl_t        * const p_ctrl);
ssp_err_t R_DTC_VersionGet (ssp_version_t     * const p_version);
ssp_err_t R_DTC_BlockReset (transfer_ctrl_t   * const p_ctrl,
                            void     const * volatile p_src,
                            void           * volatile p_dest,
                            uint16_t            const length,
                            transfer_size_t           size,
                            uint16_t            const num_transfers);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_DTC_R_DTC_PRIVATE_API_H_ */
