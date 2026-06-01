/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Abdelrahman_Abdellatif
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include <uart_driver.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "gpio_driver.h"


int main(void){

	SystemClock_Config();
	UART_Init(115200);

	UART_SendString("System started\r\n");

	char line[64];
	uint32_t counter =0;

	/*everything else will be initialized here later*/

	while(1){
		/*send a syayus message every secound*/
		UART_SendFormatted("Tick: %lu\r\n", counter++);
		Delay_ms(1000);
		/*check if a command arrived from linux*/
		if (UART_ReadLine(line, sizeof(line))){
			UART_SendFormatted("Received: %s\r\n", line);
		}
	}

}
