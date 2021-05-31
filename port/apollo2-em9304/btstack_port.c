//
// BTstack port for Apolle 2 EVB with EM9304 shield
//

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "am_devices_em9304.h"

//*****************************************************************************
//
// Insert compiler version at compile time.
//
//*****************************************************************************
#define STRINGIZE_VAL(n)                    STRINGIZE_VAL2(n)
#define STRINGIZE_VAL2(n)                   #n

#ifdef __GNUC__
#define COMPILER_VERSION                    ("GCC " __VERSION__)
#elif defined(__ARMCC_VERSION)
#define COMPILER_VERSION                    ("ARMCC " STRINGIZE_VAL(__ARMCC_VERSION))
#elif defined(__KEIL__)
#define COMPILER_VERSION                    "KEIL_CARM " STRINGIZE_VAL(__CA__)
#elif defined(__IAR_SYSTEMS_ICC__)
#define COMPILER_VERSION                    __VERSION__
#else
#define COMPILER_VERSION                    "Compiler unknown"
#endif

//*****************************************************************************
//
// IOM SPI Configuration for EM9304
//
//*****************************************************************************
const am_hal_iom_config_t g_sEm9304IOMConfigSPI =
{
    .ui32ClockFrequency = AM_HAL_IOM_8MHZ,
    .ui32InterfaceMode = AM_HAL_IOM_SPIMODE,
    .ui8WriteThreshold = 20,
    .ui8ReadThreshold = 20,
    .bSPHA = 0,
    .bSPOL = 0,
};

//*****************************************************************************
//
// UART configuration settings.
//
//*****************************************************************************
am_hal_uart_config_t g_sUartConfig =
{
    .ui32BaudRate = 115200,
    .ui32DataBits = AM_HAL_UART_DATA_BITS_8,
    .bTwoStopBits = false,
    .ui32Parity   = AM_HAL_UART_PARITY_NONE,
    .ui32FlowCtrl = AM_HAL_UART_FLOW_CTRL_NONE,
};

//*****************************************************************************
//
// Initialize the UART
//
//*****************************************************************************
void
uart_init(uint32_t ui32Module)
{
    //
    // Make sure the UART RX and TX pins are enabled.
    //
    am_bsp_pin_enable(COM_UART_TX);
    am_bsp_pin_enable(COM_UART_RX);

    //
    // Power on the selected UART
    //
    am_hal_uart_pwrctrl_enable(ui32Module);

    //
    // Start the UART interface, apply the desired configuration settings, and
    // enable the FIFOs.
    //
    am_hal_uart_clock_enable(ui32Module);

    //
    // Disable the UART before configuring it.
    //
    am_hal_uart_disable(ui32Module);

    //
    // Configure the UART.
    //
    am_hal_uart_config(ui32Module, &g_sUartConfig);

    //
    // Enable the UART FIFO.
    //
    am_hal_uart_fifo_config(ui32Module, AM_HAL_UART_TX_FIFO_1_2 | AM_HAL_UART_RX_FIFO_1_2);

    //
    // Enable the UART.
    //
    am_hal_uart_enable(ui32Module);
}

//*****************************************************************************
//
// Disable the UART
//
//*****************************************************************************
void
uart_disable(uint32_t ui32Module)
{
    //
    // Clear all interrupts before sleeping as having a pending UART interrupt
    // burns power.
    //
    am_hal_uart_int_clear(ui32Module, 0xFFFFFFFF);

    //
    // Disable the UART.
    //
    am_hal_uart_disable(ui32Module);

    //
    // Disable the UART pins.
    //
    am_bsp_pin_disable(COM_UART_TX);
    am_bsp_pin_disable(COM_UART_RX);

    //
    // Disable the UART clock.
    //
    am_hal_uart_clock_disable(ui32Module);
}

//*****************************************************************************
//
// Initialize the EM9304 BLE Controller
//
//*****************************************************************************
void
am_devices_em9304_spi_init(uint32_t ui32Module, const am_hal_iom_config_t *psIomConfig)
{
    if ( AM_REGn(IOMSTR, ui32Module, CFG) & AM_REG_IOMSTR_CFG_IFCEN_M )
    {
        return;
    }

#if defined(AM_PART_APOLLO2)
    am_hal_iom_pwrctrl_enable(ui32Module);
#endif
    //
    // Setup the pins for SPI mode.
    //
    am_bsp_iom_spi_pins_enable(ui32Module);

    //
    // Set the required configuration settings for the IOM.
    //
    am_hal_iom_config(ui32Module, psIomConfig);

    // Enable spi
    am_hal_iom_enable(ui32Module);
}

void
configure_em9304_pins(void)
{
    am_bsp_pin_enable(EM9304_CS);
    am_bsp_pin_enable(EM9304_INT);

    am_hal_gpio_out_bit_set(AM_BSP_GPIO_EM9304_CS);

    am_hal_gpio_int_polarity_bit_set(AM_BSP_GPIO_EM9304_INT, AM_HAL_GPIO_RISING);
    am_hal_gpio_int_clear(AM_HAL_GPIO_BIT(AM_BSP_GPIO_EM9304_INT));
    am_hal_gpio_int_enable(AM_HAL_GPIO_BIT(AM_BSP_GPIO_EM9304_INT));
}

void
em9304_init(void)
{
    //
    // Assert RESET to the Telink device.
    //
    am_hal_gpio_pin_config(AM_BSP_GPIO_EM9304_RESET, AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_out_bit_clear(AM_BSP_GPIO_EM9304_RESET);

    //
    // Setup SPI interface for EM9304
    //
    configure_em9304_pins();
    am_devices_em9304_spi_init(AM_BSP_EM9304_IOM, &g_sEm9304IOMConfigSPI);

    //
    // Delay for 20ms to make sure the em device gets ready for commands.
    //
    am_util_delay_ms(5);

    //
    // Enable the IOM and GPIO interrupt handlers.
    //
    am_hal_gpio_out_bit_set(AM_BSP_GPIO_EM9304_RESET);

    am_util_delay_ms(20);
}

// hal_cpu.h implementation
#include "hal_cpu.h"

void hal_cpu_disable_irqs(void){
    am_hal_interrupt_master_disable();
}

void hal_cpu_enable_irqs(void){
    am_hal_interrupt_master_enable();
}

void hal_cpu_enable_irqs_and_sleep(void){
    am_hal_interrupt_master_enable();
    __asm__("wfe"); // go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
}


// hal_time_ms.h
#include "hal_time_ms.h"
uint32_t hal_time_ms(void){
    return am_hal_stimer_counter_get();
}


/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 * @param file
 * @param ptr
 * @param len
 * @return
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
int _write(int file, char *ptr, int len);
int _write(int file, char *ptr, int len){
#if 1
  uint8_t cr = '\r';
  int i;

  if (file == STDOUT_FILENO || file == STDERR_FILENO) {
    for (i = 0; i < len; i++) {
      if (ptr[i] == '\n') {
        am_hal_uart_char_transmit_polled( AM_BSP_UART_PRINT_INST, cr );
    }
    am_hal_uart_char_transmit_polled( AM_BSP_UART_PRINT_INST, ptr[i]);
}
return i;
}
errno = EIO;
return -1;
#else
  return len;
#endif
}
int _read(int file, char * ptr, int len){
  (void)file;
  (void)ptr;
  (void)len;
  return -1;
}

int _close(int file){
  (void)file;
  return -1;
}

int _isatty(int file){
  (void)file;
  return -1;
}

int _lseek(int file){
  (void)file;
  return -1;
}

int _fstat(int file){
  (void)file;
  return -1;
}

void * _sbrk(intptr_t increment){
  return (void*) -1;
}


// hal_em9304_spi.h
#include "hal_em9304_spi.h"

static void (*hal_em9304_spi_transfer_done_callback)(void);
static void (*hal_em9304_spi_ready_callback)(void);

#if (0 == AM_BSP_EM9304_IOM)
void
am_iomaster0_isr(void)
{
    uint32_t ui32IntStatus;

    //
    // Read and clear the interrupt status.
    //
    ui32IntStatus = am_hal_iom_int_status_get(0, false);
    am_hal_iom_int_clear(0, ui32IntStatus);

    //
    // Service FIFO interrupts as necessary, and call IOM callbacks as
    // transfers are completed.
    //
    am_hal_iom_int_service(0, ui32IntStatus);
}
#endif

#if defined(AM_PART_APOLLO2)
#if (5 == AM_BSP_EM9304_IOM)
void
am_iomaster5_isr(void)
{
    uint32_t ui32IntStatus;

    //
    // Read and clear the interrupt status.
    //
    ui32IntStatus = am_hal_iom_int_status_get(5, false);
    am_hal_iom_int_clear(5, ui32IntStatus);

    //
    // Service FIFO interrupts as necessary, and call IOM callbacks as
    // transfers are completed.
    //
    am_hal_iom_int_service(5, ui32IntStatus);
}
#endif
#endif

void
am_gpio_isr(void)
{
    uint64_t ui64Status;

    //
    // Check and clear the GPIO interrupt status
    //
    ui64Status = am_hal_gpio_int_status_get(true);
    am_hal_gpio_int_clear(ui64Status);

    //
    // Check to see if this was a wakeup event from the BLE radio.
    //
    if ( ui64Status & AM_HAL_GPIO_BIT(AM_BSP_GPIO_EM9304_INT) )
    {
        if (hal_em9304_spi_ready_callback){
            (*hal_em9304_spi_ready_callback)();
        }
    }
}

void hal_em9304_spi_enable_ready_interrupt(void){
    am_hal_gpio_int_enable(AM_HAL_GPIO_BIT(AM_BSP_GPIO_EM9304_INT));
}

void hal_em9304_spi_disable_ready_interrupt(void){
    am_hal_gpio_int_disable(AM_HAL_GPIO_BIT(AM_BSP_GPIO_EM9304_INT));
}

void hal_em9304_spi_set_ready_callback(void (*done)(void)){
    hal_em9304_spi_ready_callback = done;
}

int hal_em9304_spi_get_ready(void){
    return am_hal_gpio_input_read() & AM_HAL_GPIO_BIT(AM_BSP_GPIO_EM9304_INT);
}

void hal_em9304_spi_init(void){
    hal_em9304_spi_disable_ready_interrupt();
}

void hal_em9304_spi_deinit(void){
    hal_em9304_spi_disable_ready_interrupt();
}

void hal_em9304_spi_set_transfer_done_callback(void (*done)(void)){
    hal_em9304_spi_transfer_done_callback = done;
}

void hal_em9304_spi_set_chip_select(int enable){
    if (enable){
        am_hal_gpio_out_bit_clear(AM_BSP_GPIO_EM9304_CS);
    } else {
        am_hal_gpio_out_bit_set(AM_BSP_GPIO_EM9304_CS);
    }
}

void hal_em9304_spi_transceive(const uint8_t * tx_data, uint8_t * rx_data, uint16_t len){
    // TODO: handle tx_data/rx_data not aligned
    // TODO: support non-blocking full duplex
    uint32_t ui32ChipSelect = 0;
    // TODO: Use Full Duplex with Interrupt callback
    // NOTE: Full Duplex only supported on Apollo2
    // NOTE: Enabling Full Duplex causes am_hal_iom_spi_write_nq to block (as bytes ready returns number of bytes written)
    // AM_REGn(IOMSTR, ui32Module, CFG) |= AM_REG_IOMSTR_CFG_FULLDUP(1);
    am_hal_iom_spi_fullduplex_nq(AM_BSP_EM9304_IOM, ui32ChipSelect, (uint32_t *) tx_data, (uint32_t *) rx_data, len, AM_HAL_IOM_RAW);
    (*hal_em9304_spi_transfer_done_callback)();
    return;
}

void hal_em9304_spi_transmit(const uint8_t * tx_data, uint16_t len){
    // TODO: handle tx_data/rx_data not aligned
    uint32_t ui32ChipSelect = 0;
    am_hal_iom_spi_write_nb(AM_BSP_EM9304_IOM, ui32ChipSelect, (uint32_t *) tx_data, len, AM_HAL_IOM_RAW, hal_em9304_spi_transfer_done_callback);
}

void hal_em9304_spi_receive(uint8_t * rx_data, uint16_t len){
    // TODO: handle tx_data/rx_data not aligned
    // TODO: support non-blocking full duplex
    uint32_t ui32ChipSelect = 0;
    am_hal_iom_spi_read_nb(AM_BSP_EM9304_IOM, ui32ChipSelect, (uint32_t *) rx_data, len, AM_HAL_IOM_RAW, hal_em9304_spi_transfer_done_callback);
}

int hal_em9304_spi_get_fullduplex_support(void){
    return 0;
}

//*****************************************************************************
//
// Main
//
//*****************************************************************************


// EM 9304 SPI Master HCI Implementation
const uint8_t hci_reset_2[] = { 0x01, 0x03, 0x0c, 0x00 };

#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "hci_dump.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_transport.h"
#include "hci_transport_em9304_spi.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
int btstack_main(int argc, char ** argv);

// main.c
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(size);
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            printf("BTstack up and running.\n");
            break;
        default:
            break;
    }
}

int main(void)
{
    //
    // Set the clock frequency.
    //
    am_hal_clkgen_sysclk_select(AM_HAL_CLKGEN_SYSCLK_MAX);

    //
    // Set the default cache configuration
    //
    am_hal_cachectrl_enable(&am_hal_cachectrl_defaults);

    //
    // Configure the board for low power operation.
    //
    am_bsp_low_power_init();

    //
    // Initialize the printf interface for UART output.
    //
    am_util_stdio_printf_init((am_util_stdio_print_char_t)am_bsp_uart_string_print);

    //
    // Configure and enable the UART.
    //
    uart_init(AM_BSP_UART_PRINT_INST);

    //
    // Reboot and configure em9304.
    //
    em9304_init();

    am_hal_interrupt_enable(AM_HAL_INTERRUPT_GPIO);

    //
    // Enable IOM SPI interrupts.
    //
    am_hal_iom_int_clear(AM_BSP_EM9304_IOM, AM_HAL_IOM_INT_CMDCMP | AM_HAL_IOM_INT_THR);
    am_hal_iom_int_enable(AM_BSP_EM9304_IOM, AM_HAL_IOM_INT_CMDCMP | AM_HAL_IOM_INT_THR);

#if (0 == AM_BSP_EM9304_IOM)
      am_hal_interrupt_enable(AM_HAL_INTERRUPT_IOMASTER0);
#elif (5 == AM_BSP_EM9304_IOM)
      am_hal_interrupt_enable(AM_HAL_INTERRUPT_IOMASTER5);
#endif

    // Start System Timer (only Apollo 2)
    am_hal_stimer_config(AM_HAL_STIMER_LFRC_1KHZ);
    am_hal_stimer_counter_clear();
    
    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // init HCI
    hci_init(hci_transport_em9304_spi_instance(btstack_em9304_spi_embedded_instance()), NULL);
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over control to btstack_main()..

    // turn on!
    // hci_power_control(HCI_POWER_ON);
    btstack_main(0, NULL);

    btstack_run_loop_execute();
}
