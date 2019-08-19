# Maintainer Notes

With STM32CubeMX:
- configure Bluetooth UART

## Update STM32F4 Cube HAL
- Open STM32CubeMX project
- Enable: SPI, I2S, I2C
- Generate Code
- Commit updated drivers
- Revert STM32CubeMX project

We do this as the Audio BSP configures I2S, I2C manually

