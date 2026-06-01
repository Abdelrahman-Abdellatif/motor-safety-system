#include "gpio_driver.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <uart_driver.h>

/*----------------------------------------------------------------
 * Ring buffer — private to this file (static = not visible outside)
 *
 * volatile tells the compiler: this data can change at any time
 * from an interrupt. Do not optimize or cache it in a register.
 * Always read it fresh from memory.
 *----------------------------------------------------------------*/

static volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t rx_write_idx = 0; /*interrupt writes here*/
static volatile uint16_t rx_read_idx = 0; /*main loop reads here*/

/*----------------------------------------------------------------
 * UART_Init
 *
 * Sequence :
 *   1. Enable clocks — GPIO port A and USART2
 *   2. Configure PA2 (TX) and PA3 (RX) as Alternate Function AF7
 *   3. Set baud rate in BRR
 *   4. Configure CR1 — word length, enable TX, enable RX
 *   5. Enable RX interrupt in CR1
 *   6. Enable USART2 interrupt in NVIC
 *   7. Enable the USART
 *----------------------------------------------------------------*/

void UART_Init(uint32_t baudrate){
    /*
     * Step 1 — Enable clocks
     * USART2 is on APB1 bus → RCC->APB1ENR
     * GPIOA clock is handled inside GPIO_Init via GPIO_ClockEnable
     */
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    /*
     * Step 2 — Configure PA2 (TX) and PA3 (RX) as Alternate Function
     *
     * Mode must be GPIO_MODE_AF (0x02) — not output, not input
     * Speed set to HIGH because UART toggles fast at high baud rates
     * TX pin: no pull needed (driven actively)
     * RX pin: pull-up to keep line idle HIGH when nothing connected
     */
	GPIO_PinConfig_t tx_pin = {
		.port	= GPIOA,
		.pin	= 2,
		.mode	= GPIO_MODE_AF,
		.otype	= GPIO_OTYPE_PUSHPULL,
		.speed	= GPIO_SPEED_HIGH,
		.pull	= GPIO_PULL_NONE
	};
	GPIO_Init(&tx_pin);

	GPIO_PinConfig_t rx_pin ={
		.port	= GPIOA,
		.pin	= 3,
		.mode	= GPIO_MODE_AF,
		.otype	= GPIO_OTYPE_PUSHPULL,
		.speed	= GPIO_SPEED_HIGH,
		.pull	= GPIO_PULL_UP
	};
	GPIO_Init(&rx_pin);

    /*
     * Step 2b — Set Alternate Function number to AF7 (USART2)
     *
     * AFR is an array of 2 registers: AFR[0] covers pins 0-7
     *                                 AFR[1] covers pins 8-15
     * Each pin gets 4 bits in AFR
     * PA2 → AFR[0], bit position = pin * 4 = 2 * 4 = 8
     * PA3 → AFR[0], bit position = pin * 4 = 3 * 4 = 12
     * AF7 = 0x07
     *
     * Why AF7? Check the STM32F401 datasheet Table 9 — Alternate
     * function mapping. USART2 TX/RX = AF7 for PA2/PA3.
     */

	GPIOA->AFR[0] &= ~(0xFUL << (2*4)); /*clears PA2 AF bits*/
	GPIOA->AFR[0] |=  (0x7UL << (2*4)); /*set PA2 to AF7*/

	GPIOA->AFR[0] &= ~(0xFUL << (3*4)); /*clears PA3 AF bits*/
	GPIOA->AFR[0] |=  (0x7UL << (3*4)); /*set PA3 to AF7*/


    /*
     * Step 3 — Set baud rate
     *
     * USART2 is clocked from APB1 = 42MHz in our system
     * Formula: USARTDIV = FCLK / (16 × baudrate)
     * instead of truncating — gives more accurate baud rate
     */
	/*uint32_t apb1_clk = 42000000UL;
	USART2->BRR = (apb1_clk + (baudrate / 2 )) / baudrate; */
    uint32_t ppre1 = (RCC->CFGR >> 10) & 0x7UL;
    uint32_t apb1_clk;

    if (ppre1 < 4)
    {
        apb1_clk = SystemCoreClock;
    }
    else
    {
        uint32_t divisor = (1UL << (ppre1 - 3));
        apb1_clk = SystemCoreClock / divisor;
    }

    /* +baudrate/2 is for rounding to nearest integer */
    USART2->BRR = (apb1_clk + (baudrate / 2)) / baudrate;

    /*
     * Step 4 — Configure CR1
     *
     * CR1_TE  = Transmitter Enable
     * CR1_RE  = Receiver Enable
     * Word length bit (M) = 0 → 8 data bits (default, leave it)
     * Parity = disabled (default, leave it)
     *
     * We do NOT enable the USART yet (UE bit) — do that last
     */
	USART2->CR1 = 0; /*start from  a clean state*/
	USART2->CR1 |= USART_CR1_TE; /*Enables transmitter*/
	USART2->CR1 |= USART_CR1_RE;  /*Enables reciver*/

	/*
     * Step 5 — Enable RX Not Empty interrupt
     *
     * RXNEIE = RX Not Empty Interrupt Enable
     * When a byte arrives in DR, this fires USART2_IRQHandler
     */
	USART2->CR1 |= USART_CR1_RXNEIE;

    /*
     * Step 6 — Enable USART2 interrupt in NVIC
     *
     * NVIC is the interrupt controller inside the Cortex-M4
     * Without this, the interrupt fires inside USART2 hardware
     * but never reaches your handler function
     * Priority 0 = highest. For UART RX this is fine.
     */
	NVIC_SetPriority(USART2_IRQn, 0);
	NVIC_EnableIRQ(USART2_IRQn);

    /*
     * Step 7 — Enable the USART peripheral
     * UE = USART Enable — do this last, after everything configured
     */
	USART2->CR1 |= USART_CR1_UE;
}
/*----------------------------------------------------------------
 * UART_SendByte
 *
 * TXE flag in SR = 1 means transmit data register is empty
 * and ready to accept a new byte
 * We wait for TXE, then write to DR
 * Writing to DR automatically clears TXE and starts transmission
 *----------------------------------------------------------------*/
void UART_SendByte(uint8_t byte){
	while (!(USART2->SR & USART_SR_TXE));
	USART2->DR =byte;
}
/*----------------------------------------------------------------
 * UART_SendString
 *
 * Walk through the string one char at a time until null terminator
 *----------------------------------------------------------------*/
void UART_SendString(const char *str){
	while (*str){
		UART_SendByte((uint8_t)(*str));
		str++;
	}
}


/*----------------------------------------------------------------
 * UART_tedted
 *
 * Uses vsnprintf from standard C library to format the string
 * into a local buffer first, then sends it
 * This gives you printf-style formatting over UART
 *----------------------------------------------------------------*/
void UART_SendFormatted(const char *fmt, ...){
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    UART_SendString(buf);
}

/*----------------------------------------------------------------
 * USART2_IRQHandler
 *
 * This function name is FIXED — it must match exactly
 * It is declared as weak in the startup file
 * Our definition here overrides it automatically
 *
 * This runs every time a byte arrives on RX
 * We do ONE thing here: store byte in ring buffer and move index
 * Never do heavy work inside an interrupt handler
 *----------------------------------------------------------------*/
void USART2_IRQHandler(void){
    /*
     * Check RXNE flag — RX Not Empty
     * This confirms the interrupt is from a received byte
     * (not some other USART interrupt source)
     */
	if (USART2->SR & USART_SR_RXNE){
        /*
         * Reading DR automatically clears the RXNE flag
         * Store byte in ring buffer at current write position
         * Advance write index and wrap using bitmask
         *
         * The & (UART_RX_BUFFER_SIZE - 1) trick:
         * If buffer size is 128 (binary: 10000000)
         * then size-1 = 127 (binary: 01111111)
         * ANDing with it keeps index within 0-127 forever
         * This only works when size is a power of 2
         */
		rx_buffer[rx_write_idx] = (uint8_t)(USART2->DR);
		rx_write_idx =(rx_write_idx + 1) & (UART_RX_BUFFER_SIZE -1);

	}
}
/*----------------------------------------------------------------
 * UART_RxAvailable
 *
 * Returns number of bytes waiting in the ring buffer
 * If write == read, buffer is empty
 *----------------------------------------------------------------*/
uint16_t UART_RxAvailable(void){
	return (rx_write_idx - rx_read_idx) & (UART_RX_BUFFER_SIZE - 1);
}

/*----------------------------------------------------------------
 * UART_ReadLine
 *
 * Scans ring buffer for a '\n' character
 * If found: copies everything up to and including '\n' into buf
 *           advances read index past those bytes
 *           returns 1
 * If not found yet: returns 0, buffer unchanged
 *
 * Call this from your main loop — it never blocks
 *----------------------------------------------------------------*/

uint8_t UART_ReadLine(char *buf, uint16_t maxlen){

	uint16_t available = UART_RxAvailable();

	if (available ==0) return 0;
    /*
     * Scan forward from read index looking for newline
     * We scan a copy of the index so we do not move the real one
     * until we are sure a complete line is there
     */
	uint16_t scan_idx = rx_read_idx;
	uint16_t count	= 0;

	while (count <available){
		uint8_t byte = rx_buffer[(scan_idx + count) & (UART_RX_BUFFER_SIZE - 1)];
		count++;

		if (byte== '\n'){
			/*Found a complete line - copy it into buf*/
			uint16_t copy_count = (count < maxlen) ? count : (maxlen -1);

			for (uint16_t i =0; i < copy_count; i++){
				buf[i] = rx_buffer[(rx_read_idx + i) & (UART_RX_BUFFER_SIZE - 1)];
			}
			buf[copy_count]='\0';

			/*Advance the real read index past this line*/
			rx_read_idx = (rx_read_idx + count) & (UART_RX_BUFFER_SIZE -1);
			return 1;
		}
	}

	return 0; /*no complete line yet*/
}


















