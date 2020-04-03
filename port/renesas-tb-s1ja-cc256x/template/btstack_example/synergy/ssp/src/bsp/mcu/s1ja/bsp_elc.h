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
/***********************************************************************************************************************
* File Name    : bsp_elc.h
* Description  : ELC Interface.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @addtogroup ELC_API events and peripheral definitions
 * @brief Interface for the Event Link Controller.
 *
 * Related SSP architecture topics:
 *  - What is an SSP Interface? @ref ssp-interfaces
 *  - What is a SSP Layer? @ref ssp-predefined-layers
 *  - How to use SSP Interfaces and Modules? @ref using-ssp-modules
 *
 * Event Link Controller Interface description: @ref HALELCInterface
 *
 * @{
 **********************************************************************************************************************/

/** @} (end addtogroup ELC_API) */

#ifndef BSP_ELCDEFS_H_
#define BSP_ELCDEFS_H_

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/
/** Possible peripherals to be linked to event signals */
typedef enum e_elc_peripheral
{
    ELC_PERIPHERAL_GPT_A                                                        =    (0),
    ELC_PERIPHERAL_GPT_B                                                        =    (1),
    ELC_PERIPHERAL_GPT_C                                                        =    (2),
    ELC_PERIPHERAL_GPT_D                                                        =    (3),
    ELC_PERIPHERAL_ADC0                                                         =    (8),
    ELC_PERIPHERAL_ADC0_B                                                       =    (9),
    ELC_PERIPHERAL_IOPORT1                                                      =   (14),
    ELC_PERIPHERAL_IOPORT2                                                      =   (15),
    ELC_PERIPHERAL_CTSU                                                         =   (18),
    ELC_PERIPHERAL_DA80                                                         =   (19),
    ELC_PERIPHERAL_DA81                                                         =   (20),
    ELC_PERIPHERAL_SDADC0                                                       =   (22),
} elc_peripheral_t;

/** Sources of event signals to be linked to other peripherals or the CPU1
 * @note This list may change based on device. This list is for S1JA.
 * */
typedef enum e_elc_event
{
    ELC_EVENT_ICU_IRQ0                                                          =    (1),
    ELC_EVENT_ICU_IRQ1                                                          =    (2),
    ELC_EVENT_ICU_IRQ2                                                          =    (3),
    ELC_EVENT_ICU_IRQ3                                                          =    (4),
    ELC_EVENT_ICU_IRQ4                                                          =    (5),
    ELC_EVENT_ICU_IRQ5                                                          =    (6),
    ELC_EVENT_ICU_IRQ6                                                          =    (7),
    ELC_EVENT_ICU_IRQ7                                                          =    (8),
    ELC_EVENT_DTC_COMPLETE                                                      =    (9),
    ELC_EVENT_DTC_END                                                           =   (10),
    ELC_EVENT_ICU_SNOOZE_CANCEL                                                 =   (11),
    ELC_EVENT_FCU_FRDYI                                                         =   (12),
    ELC_EVENT_LVD_LVD1                                                          =   (13),
    ELC_EVENT_LVD_LVD2                                                          =   (14),
    ELC_EVENT_CGC_MOSC_STOP                                                     =   (15),
    ELC_EVENT_LPM_SNOOZE_REQUEST                                                =   (16),
    ELC_EVENT_AGT0_INT                                                          =   (17),
    ELC_EVENT_AGT0_COMPARE_A                                                    =   (18),
    ELC_EVENT_AGT0_COMPARE_B                                                    =   (19),
    ELC_EVENT_AGT1_INT                                                          =   (20),
    ELC_EVENT_AGT1_COMPARE_A                                                    =   (21),
    ELC_EVENT_AGT1_COMPARE_B                                                    =   (22),
    ELC_EVENT_IWDT_UNDERFLOW                                                    =   (23),
    ELC_EVENT_WDT_UNDERFLOW                                                     =   (24),
    ELC_EVENT_RTC_ALARM                                                         =   (25),
    ELC_EVENT_RTC_PERIOD                                                        =   (26),
    ELC_EVENT_RTC_CARRY                                                         =   (27),
    ELC_EVENT_ADC0_SCAN_END                                                     =   (28),
    ELC_EVENT_ADC0_SCAN_END_B                                                   =   (29),
    ELC_EVENT_ADC0_WINDOW_A                                                     =   (30),
    ELC_EVENT_ADC0_WINDOW_B                                                     =   (31),
    ELC_EVENT_ADC0_COMPARE_MATCH                                                =   (32),
    ELC_EVENT_ADC0_COMPARE_MISMATCH                                             =   (33),
    ELC_EVENT_COMP_HS0_INT                                                      =   (34),
    ELC_EVENT_COMP_LP0_INT                                                      =   (35),
    ELC_EVENT_COMP_LP1_INT                                                      =   (36),
    ELC_EVENT_USBFS_INT                                                         =   (37),
    ELC_EVENT_USBFS_RESUME                                                      =   (38),
    ELC_EVENT_IIC0_RXI                                                          =   (39),
    ELC_EVENT_IIC0_TXI                                                          =   (40),
    ELC_EVENT_IIC0_TEI                                                          =   (41),
    ELC_EVENT_IIC0_ERI                                                          =   (42),
    ELC_EVENT_IIC0_WUI                                                          =   (43),
    ELC_EVENT_IIC1_RXI                                                          =   (44),
    ELC_EVENT_IIC1_TXI                                                          =   (45),
    ELC_EVENT_IIC1_TEI                                                          =   (46),
    ELC_EVENT_IIC1_ERI                                                          =   (47),
    ELC_EVENT_CTSU_WRITE                                                        =   (48),
    ELC_EVENT_CTSU_READ                                                         =   (49),
    ELC_EVENT_CTSU_END                                                          =   (50),
    ELC_EVENT_KEY_INT                                                           =   (51),
    ELC_EVENT_DOC_INT                                                           =   (52),
    ELC_EVENT_CAC_FREQUENCY_ERROR                                               =   (53),
    ELC_EVENT_CAC_MEASUREMENT_END                                               =   (54),
    ELC_EVENT_CAC_OVERFLOW                                                      =   (55),
    ELC_EVENT_CAN0_ERROR                                                        =   (56),
    ELC_EVENT_CAN0_FIFO_RX                                                      =   (57),
    ELC_EVENT_CAN0_FIFO_TX                                                      =   (58),
    ELC_EVENT_CAN0_MAILBOX_RX                                                   =   (59),
    ELC_EVENT_CAN0_MAILBOX_TX                                                   =   (60),
    ELC_EVENT_IOPORT_EVENT_1                                                    =   (61),
    ELC_EVENT_IOPORT_EVENT_2                                                    =   (62),
    ELC_EVENT_ELC_SOFTWARE_EVENT_0                                              =   (63),
    ELC_EVENT_ELC_SOFTWARE_EVENT_1                                              =   (64),
    ELC_EVENT_POEG0_EVENT                                                       =   (65),
    ELC_EVENT_POEG1_EVENT                                                       =   (66),
    ELC_EVENT_SDADC0_ADI                                                        =   (67),
    ELC_EVENT_SDADC0_SCANEND                                                    =   (68),
    ELC_EVENT_SDADC0_CALIEND                                                    =   (69),
    ELC_EVENT_GPT0_CAPTURE_COMPARE_A                                            =   (70),
    ELC_EVENT_GPT0_CAPTURE_COMPARE_B                                            =   (71),
    ELC_EVENT_GPT0_COMPARE_C                                                    =   (72),
    ELC_EVENT_GPT0_COMPARE_D                                                    =   (73),
    ELC_EVENT_GPT0_COUNTER_OVERFLOW                                             =   (74),
    ELC_EVENT_GPT0_COUNTER_UNDERFLOW                                            =   (75),
    ELC_EVENT_GPT1_CAPTURE_COMPARE_A                                            =   (76),
    ELC_EVENT_GPT1_CAPTURE_COMPARE_B                                            =   (77),
    ELC_EVENT_GPT1_COMPARE_C                                                    =   (78),
    ELC_EVENT_GPT1_COMPARE_D                                                    =   (79),
    ELC_EVENT_GPT1_COUNTER_OVERFLOW                                             =   (80),
    ELC_EVENT_GPT1_COUNTER_UNDERFLOW                                            =   (81),
    ELC_EVENT_GPT2_CAPTURE_COMPARE_A                                            =   (82),
    ELC_EVENT_GPT2_CAPTURE_COMPARE_B                                            =   (83),
    ELC_EVENT_GPT2_COMPARE_C                                                    =   (84),
    ELC_EVENT_GPT2_COMPARE_D                                                    =   (85),
    ELC_EVENT_GPT2_COUNTER_OVERFLOW                                             =   (86),
    ELC_EVENT_GPT2_COUNTER_UNDERFLOW                                            =   (87),
    ELC_EVENT_GPT3_CAPTURE_COMPARE_A                                            =   (88),
    ELC_EVENT_GPT3_CAPTURE_COMPARE_B                                            =   (89),
    ELC_EVENT_GPT3_COMPARE_C                                                    =   (90),
    ELC_EVENT_GPT3_COMPARE_D                                                    =   (91),
    ELC_EVENT_GPT3_COUNTER_OVERFLOW                                             =   (92),
    ELC_EVENT_GPT3_COUNTER_UNDERFLOW                                            =   (93),
    ELC_EVENT_GPT4_CAPTURE_COMPARE_A                                            =   (94),
    ELC_EVENT_GPT4_CAPTURE_COMPARE_B                                            =   (95),
    ELC_EVENT_GPT4_COMPARE_C                                                    =   (96),
    ELC_EVENT_GPT4_COMPARE_D                                                    =   (97),
    ELC_EVENT_GPT4_COUNTER_OVERFLOW                                             =   (98),
    ELC_EVENT_GPT4_COUNTER_UNDERFLOW                                            =   (99),
    ELC_EVENT_GPT5_CAPTURE_COMPARE_A                                            =  (100),
    ELC_EVENT_GPT5_CAPTURE_COMPARE_B                                            =  (101),
    ELC_EVENT_GPT5_COMPARE_C                                                    =  (102),
    ELC_EVENT_GPT5_COMPARE_D                                                    =  (103),
    ELC_EVENT_GPT5_COUNTER_OVERFLOW                                             =  (104),
    ELC_EVENT_GPT5_COUNTER_UNDERFLOW                                            =  (105),
    ELC_EVENT_GPT6_CAPTURE_COMPARE_A                                            =  (106),
    ELC_EVENT_GPT6_CAPTURE_COMPARE_B                                            =  (107),
    ELC_EVENT_GPT6_COMPARE_C                                                    =  (108),
    ELC_EVENT_GPT6_COMPARE_D                                                    =  (109),
    ELC_EVENT_GPT6_COUNTER_OVERFLOW                                             =  (110),
    ELC_EVENT_GPT6_COUNTER_UNDERFLOW                                            =  (111),
    ELC_EVENT_OPS_UVW_EDGE                                                      =  (112),
    ELC_EVENT_SCI0_RXI                                                          =  (113),
    ELC_EVENT_SCI0_TXI                                                          =  (114),
    ELC_EVENT_SCI0_TEI                                                          =  (115),
    ELC_EVENT_SCI0_ERI                                                          =  (116),
    ELC_EVENT_SCI0_AM                                                           =  (117),
    ELC_EVENT_SCI0_RXI_OR_ERI                                                   =  (118),
    ELC_EVENT_SCI1_RXI                                                          =  (119),
    ELC_EVENT_SCI1_TXI                                                          =  (120),
    ELC_EVENT_SCI1_TEI                                                          =  (121),
    ELC_EVENT_SCI1_ERI                                                          =  (122),
    ELC_EVENT_SCI1_AM                                                           =  (123),
    ELC_EVENT_SCI9_RXI                                                          =  (124),
    ELC_EVENT_SCI9_TXI                                                          =  (125),
    ELC_EVENT_SCI9_TEI                                                          =  (126),
    ELC_EVENT_SCI9_ERI                                                          =  (127),
    ELC_EVENT_SCI9_AM                                                           =  (128),
    ELC_EVENT_SPI0_RXI                                                          =  (129),
    ELC_EVENT_SPI0_TXI                                                          =  (130),
    ELC_EVENT_SPI0_IDLE                                                         =  (131),
    ELC_EVENT_SPI0_ERI                                                          =  (132),
    ELC_EVENT_SPI0_TEI                                                          =  (133),
    ELC_EVENT_SPI1_RXI                                                          =  (134),
    ELC_EVENT_SPI1_TXI                                                          =  (135),
    ELC_EVENT_SPI1_IDLE                                                         =  (136),
    ELC_EVENT_SPI1_ERI                                                          =  (137),
    ELC_EVENT_SPI1_TEI                                                          =  (138),
    ELC_EVENT_AES_WRREQ                                                         =  (139),
    ELC_EVENT_AES_RDREQ                                                         =  (140),
    ELC_EVENT_TRNG_RDREQ                                                        =  (141),
} elc_event_t;

#endif /* BSP_ELCDEFS_H_ */
