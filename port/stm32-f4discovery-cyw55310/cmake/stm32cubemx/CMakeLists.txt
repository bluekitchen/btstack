cmake_minimum_required(VERSION 3.22)

project(stm32cubemx)
add_library(stm32cubemx INTERFACE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(stm32cubemx INTERFACE 
	USE_HAL_DRIVER 
	STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>
)

# Ignore warnings in STM32CubeMX code
target_compile_options(stm32cubemx INTERFACE -Wno-unused-parameter)

target_include_directories(stm32cubemx INTERFACE
    ../../Inc
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${STM32F4CUBE_PATH}/Middlewares/ST/STM32_Audio/Addons/PDM/Inc
    ${STM32F4CUBE_PATH}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${STM32F4CUBE_PATH}/Drivers/CMSIS/Include
)

target_sources(stm32cubemx INTERFACE
    ../../Src/main.c
    ../../Src/gpio.c
    ../../Src/crc.c
    ../../Src/dma.c
    ../../Src/i2c.c
    ../../Src/i2s.c
    ../../Src/pdm2pcm.c
    ../../Src/spi.c
    ../../Src/usart.c
    ../../Src/usb_otg.c
    ../../Src/stm32f4xx_it.c
    ../../Src/stm32f4xx_hal_msp.c
	../../Src/system_stm32f4xx.c
	../../Src/sysmem.c
	../../Src/syscalls.c
	../../startup_stm32f407xx.s

	# STM32CubeF4
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_crc.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2s.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2s_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${STM32F4CUBE_PATH}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)

target_link_directories(stm32cubemx INTERFACE
    ${STM32F4CUBE_PATH}/Middlewares/ST/STM32_Audio/Addons/PDM/Lib
    ${STM32F4CUBE_PATH}/Middlewares/ST/STM32_Audio/Addons/PDM/Lib
)

target_link_libraries(stm32cubemx INTERFACE 
	:libPDMFilter_CM4_GCC_wc32.a 
	:libPDMFilter_CM4_IAR_wc32.a
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()


