/** 
 * @file  hal_bt.c
 ***************************************************************************/
#include <stdint.h>

#include <msp430x54x.h>
#include "hal_compat.h"

#include <btstack/hal_uart_dma.h>

extern void hal_cpu_set_uart_needed_during_sleep(uint8_t enabled);

// debugging only
// #include <stdio.h>

#define BT_PORT_OUT      P9OUT
#define BT_PORT_SEL      P9SEL
#define BT_PORT_DIR      P9DIR
#define BT_PORT_REN      P9REN
#define BT_PIN_TXD       BIT4
#define BT_PIN_RXD       BIT5

// RXD P9.5
// TXD P9.4
// RTS P1.4
// CTS P1.3

void dummy_handler(void){};

// rx state
static uint16_t  bytes_to_read = 0;
static uint8_t * rx_buffer_ptr = 0;

// tx state
static uint16_t  bytes_to_write = 0;
static uint8_t * tx_buffer_ptr = 0;

// handlers
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;
static void (*cts_irq_handler)(void) = dummy_handler;

/**
 * @brief  Initializes the serial communications peripheral and GPIO ports 
 *         to communicate with the PAN BT .. assuming 16 Mhz CPU
 * 
 * @param  none
 * 
 * @return none
 */
void hal_uart_dma_init(void)
{
    BT_PORT_SEL |= BT_PIN_RXD + BT_PIN_TXD;
    BT_PORT_DIR |= BT_PIN_TXD;
    BT_PORT_DIR &= ~BT_PIN_RXD;

    // set BT RTS (P1.4)
    P1SEL &= ~BIT4;  // = 0 - I/O
    P1DIR |=  BIT4;  // = 1 - Output
    P1OUT |=  BIT4;  // = 1 - RTS high -> stop

    // set BT CTS 
    P1SEL &= ~BIT3;  // = 0 - I/O
    P1DIR &= ~BIT3;  // = 0 - Input    P1DIR |=  BIT4; // RTS
        
    // set BT SHUTDOWN (P8.2) to 1 (active low)
    P8SEL &= ~BIT2;  // = 0 - I/O
    P8DIR |=  BIT2;  // = 1 - Output
    P8OUT |=  BIT2;  // = 1 - Active low -> ok

    // Enable ACLK to provide 32 kHz clock to Bluetooth module
    P11SEL |= BIT0;
    P11DIR |= BIT0;

    // wait for Bluetooth to power up properly after providing 32khz clock
    waitAboutOneSecond();

    UCA2CTL1 |= UCSWRST;              //Reset State                      
    UCA2CTL0 = UCMODE_0;
    
    UCA2CTL0 &= ~UC7BIT;              // 8bit char
    UCA2CTL1 |= UCSSEL_2;
    
    UCA2CTL1 &= ~UCSWRST;             // continue

    hal_uart_dma_set_baud(115200);
}

/**

 UART used in low-frequency mode
 In this mode, the maximum USCI baud rate is one-third the UART source clock frequency BRCLK.
 
 16000000 /  576000 = 277.77
 16000000 /  115200 = 138.88
 16000000 /  921600 =  17.36
 16000000 / 1000000 =  16.00
 16000000 / 2000000 =   8.00
 16000000 / 2400000 =   6.66
 16000000 / 3000000 =   3.33
 16000000 / 4000000 =   2.00
 
 */
int hal_uart_dma_set_baud(uint32_t baud){

    int result = 0;
    
    UCA2CTL1 |= UCSWRST;              //Reset State                      

    switch (baud){

        case 4000000:
            UCA2BR0 = 2;
            UCA2BR1 = 0;
            UCA2MCTL= 0 << 1;  // + 0.000
            break;
            
        case 3000000:
            UCA2BR0 = 3;
            UCA2BR1 = 0;
            UCA2MCTL= 3 << 1;  // + 0.375
            break;
            
        case 2400000:
            UCA2BR0 = 6;
            UCA2BR1 = 0;
            UCA2MCTL= 5 << 1;  // + 0.625
            break;

        case 2000000:
            UCA2BR0 = 8;
            UCA2BR1 = 0;
            UCA2MCTL= 0 << 1;  // + 0.000
            break;

        case 1000000:
            UCA2BR0 = 16;
            UCA2BR1 = 0;
            UCA2MCTL= 0 << 1;  // + 0.000
            break;
            
        case 921600:
            UCA2BR0 = 17;
            UCA2BR1 = 0;
            UCA2MCTL= 7 << 1;  // 3 << 1;  // + 0.375
            break;
            
        case 115200:
            UCA2BR0 = 138;  // from family user guide
            UCA2BR1 = 0;
            UCA2MCTL= 7 << 1;  // + 0.875
            break;

        case 57600:
            UCA2BR0 = 21;
            UCA2BR1 = 1;
            UCA2MCTL= 7 << 1;  // + 0.875
            break;

        default:
            result = -1;
            break;
    }

    UCA2CTL1 &= ~UCSWRST;             // continue
    
    return result;
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
    if (the_irq_handler){
        P1IFG  =  0;     // no IRQ pending
        P1IV   =  0;     // no IRQ pending
        P1IES &= ~BIT3;  // IRQ on 0->1 transition
        P1IE  |=  BIT3;  // enable IRQ for P1.3
        cts_irq_handler = the_irq_handler;
        return;
    }
    
    P1IE  &= ~BIT3;
    cts_irq_handler = dummy_handler;
}

/**********************************************************************/
/**
 * @brief  Disables the serial communications peripheral and clears the GPIO
 *         settings used to communicate with the BT.
 * 
 * @param  none
 * 
 * @return none
 **************************************************************************/
void hal_uart_dma_shutdown(void) {
    
    UCA2IE &= ~(UCRXIE | UCTXIE);
    UCA2CTL1 = UCSWRST;                          //Reset State                         
    BT_PORT_SEL &= ~( BT_PIN_RXD + BT_PIN_TXD );
    BT_PORT_DIR |= BT_PIN_TXD;
    BT_PORT_DIR |= BT_PIN_RXD;
    BT_PORT_OUT &= ~(BT_PIN_TXD + BT_PIN_RXD);
}

void hal_uart_dma_send_block(const uint8_t * data, uint16_t len){
    
    // printf("hal_uart_dma_send_block, size %u\n\r", len);
    
    UCA2IE &= ~UCTXIE ;  // disable TX interrupts

    tx_buffer_ptr = (uint8_t *) data;
    bytes_to_write = len;

    UCA2IE |= UCTXIE;    // enable TX interrupts
}

static inline void hal_uart_dma_enable_rx(void){
    P1OUT &= ~BIT4;  // = 0 - RTS low -> ok
}

static inline void hal_uart_dma_disable_rx(void){
    P1OUT |= BIT4;  // = 1 - RTS high -> stop
}

// int used to indicate a request for more new data
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){

    UCA2IE &= ~UCRXIE ;  // disable RX interrupts

    rx_buffer_ptr = buffer;
    bytes_to_read = len;
    
    UCA2IE |= UCRXIE;    // enable RX interrupts

    hal_uart_dma_enable_rx();     // enable receive 
}

void hal_uart_dma_set_sleep(uint8_t sleep){
    hal_cpu_set_uart_needed_during_sleep(!sleep);    
}

// block-wise "DMA" RX/TX UART driver
#ifdef __GNUC__
__attribute__((interrupt(USCI_A2_VECTOR)))
#endif
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=USCI_A2_VECTOR
__interrupt
#endif
void usbRxTxISR(void){ 

    // find reason
    switch (UCA2IV){
    
        case 2: // RXIFG
            if (bytes_to_read == 0) {
                hal_uart_dma_disable_rx();
                UCA2IE &= ~UCRXIE ;  // disable RX interrupts
                return;
            }
            *rx_buffer_ptr = UCA2RXBUF;
            ++rx_buffer_ptr;
            --bytes_to_read;
            if (bytes_to_read > 0) {
                return;
            }
            P1OUT |= BIT4;      // = 1 - RTS high -> stop
            UCA2IE &= ~UCRXIE ; // disable RX interrupts
        
            (*rx_done_handler)();
            
            // force exit low power mode
            __bic_SR_register_on_exit(LPM0_bits);   // Exit active CPU
            
            break;

        case 4: // TXIFG
            if (bytes_to_write == 0){
                UCA2IE &= ~UCTXIE ;  // disable TX interrupts
                return;
            }
            UCA2TXBUF = *tx_buffer_ptr;
            ++tx_buffer_ptr;
            --bytes_to_write;
            
            if (bytes_to_write > 0) {
                return;
            }
            
            UCA2IE &= ~UCTXIE ;  // disable TX interrupts

            (*tx_done_handler)();

            // force exit low power mode
            __bic_SR_register_on_exit(LPM0_bits);   // Exit active CPU

            break;

        default:
            break;
    }
}


// CTS ISR

extern void ehcill_handle(uint8_t action);
#define EHCILL_CTS_SIGNAL      0x034

#ifdef __GNUC__
__attribute__((interrupt(PORT1_VECTOR)))
#endif
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=PORT1_VECTOR
__interrupt
#endif
void ctsISR(void){ 
    P1IV = 0;
    (*cts_irq_handler)();
}
