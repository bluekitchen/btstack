#ifndef __BOARD__MIROMICO__L451RE_H__
#define __BOARD__MIROMICO__L451RE_H__

#define RADIO_SPI_HANDLE    hspi2

#define RADIO_NSS_PIN       GPIO_PIN_12
#define RADIO_NSS_PORT      GPIOB

#define RADIO_MOSI_PIN      GPIO_PIN_15
#define RADIO_MOSI_PORT     GPIOB

#define RADIO_MISO_PIN      GPIO_PIN_14
#define RADIO_MISO_PORT     GPIOB

#define RADIO_SCK_PIN       GPIO_PIN_10
#define RADIO_SCK_PORT      GPIOB

#define RADIO_nRESET_PIN    GPIO_PIN_5
#define RADIO_nRESET_PORT   GPIOC

#define RADIO_BUSY_PIN      GPIO_PIN_1
#define RADIO_BUSY_PORT     GPIOA

#define RADIO_DIO1_Pin 		 GPIO_PIN_6
#define RADIO_DIO1_GPIO_Port GPIOA

#define RADIO_DIO2_Pin 		 GPIO_PIN_7
#define RADIO_DIO2_GPIO_Port GPIOA

#define RADIO_DIO3_Pin 		 GPIO_PIN_4
#define RADIO_DIO3_GPIO_Port GPIOC

// #define RADIO_DIOx_PIN      GPIO_PIN_4
// #define RADIO_DIOx_PORT     GPIOB

// #define USART_TX_PIN        GPIO_PIN_2
// #define USART_TX_PORT       GPIOA

// #define USART_RX_PIN        GPIO_PIN_3
// #define USART_RX_PORT       GPIOA

// #define ANT_SW_PIN          GPIO_PIN_0
// #define ANT_SW_PORT         GPIOB

// #define LED_RX_PIN          GPIO_PIN_0
// #define LED_RX_PORT         GPIOC

// #define LED_TX_PIN          GPIO_PIN_1
// #define LED_TX_PORT         GPIOC

#endif // __BOARD__MIROMICO__L451RE_H__
