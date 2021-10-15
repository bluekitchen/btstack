/*
*********************************************************************************************************
*
*	模块名称 : STM32F4 内置ETH MAC 驱动模块
*	文件名称 : bsp_eth.h
*	版    本 : V2.4
*	说    明 : 安富莱STM32-V5开发板外扩的MAC为 DM9161
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BSP_ETH_H
#define __BSP_ETH_H

#ifdef __cplusplus
 extern "C" {
#endif

/* 定义 ETH_PHY 中断口线 PH6  used to manage Ethernet link status */
#define ETH_LINK_EXTI_LINE             EXTI_Line6
#define ETH_LINK_EXTI_PORT_SOURCE      EXTI_PortSourceGPIOH
#define ETH_LINK_EXTI_PIN_SOURCE       EXTI_PinSource6
#define ETH_LINK_EXTI_IRQn             EXTI9_5_IRQn			/* 中断号，在stm32f4xx.h 中定义 */

#define ETH_LINK_PIN                   GPIO_Pin_6
#define ETH_LINK_GPIO_PORT             GPIOH
#define ETH_LINK_GPIO_CLK              RCC_AHB1Periph_GPIOH


#define PHY_ADDRESS      0x00 	/* 地址有DM9161的RXD0 - RXD3 决定，上电复位时锁存. 安富莱STM32-V5开发板地址是0 */


/* PHY registers */
#define PHY_MICR                  0x11 	/* MII Interrupt Control Register */
#define PHY_MICR_INT_EN           ((uint16_t)0x0002) /* PHY Enable interrupts */
#define PHY_MICR_INT_OE           ((uint16_t)0x0001) /* PHY Enable output interrupt events */
#define PHY_MISR                  0x12 /* MII Interrupt Status and Misc. Control Register */
#define PHY_MISR_LINK_INT_EN      ((uint16_t)0x0020) /* Enable Interrupt on change of link status */
#define PHY_LINK_STATUS           ((uint16_t)0x2000) /* PHY link status interrupt mask */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void  ETH_BSP_Config(void);
uint32_t Eth_Link_PHYITConfig(uint16_t PHYAddress);
void Eth_Link_EXTIConfig(void);
void Eth_Link_ITHandler(uint16_t PHYAddress);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4x7_ETH_BSP_H */


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
