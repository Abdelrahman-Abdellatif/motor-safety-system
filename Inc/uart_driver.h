#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include "stm32f4xx.h"
#include <stdint.h>

/* -----------------------------------------------
 * Ring buffer size - must be power of 2 for efficient masking
 * 128 byte is enough for our command protocol
 *------------------------------------------------- */
#define UART_RX_BUFFER_SIZE		128

/* -----------------------------------------------
 * Function decleration
 *------------------------------------------------- */

/* -----------------------------------------------
 * Initializes USART2 at given baud rate
 * Configures PA2 as TX, PA3 as RX in Alternative Function mode
 * Enables RX interrupt
 *------------------------------------------------- */
void UART_Init(uint32_t baudrate);

/* -----------------------------------------------
 * Send a single byte - blocks until TX register is empty
 *------------------------------------------------- */
void UART_SendByte(uint8_t byte);

/* -----------------------------------------------
 * Sends null-terminated string byte by byte
 *------------------------------------------------- */
void UART_SendString(const char *str);

/* -----------------------------------------------
 * sends formatted string - works like printf
 *------------------------------------------------- */
void UART_SendFormatted(const char *fmt, ...);

/* -----------------------------------------------
 * Reads one complete line from RX ring buffer
 * A line ends with '\n'
 * Returns 1 if a complete line was found, 0 if not yet
 * You call this from the main loop - it never blocks
 *------------------------------------------------- */

uint8_t UART_ReadLine(char *buf, uint16_t maxlen);


/* -----------------------------------------------
 * Return how many bytes are waiting in the RX ring buffer
 *------------------------------------------------- */
uint16_t UART_RxAvailable(void);


#endif /* UART_DRIVER_H */
