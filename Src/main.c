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
#include "stm32f4xx.h"
#include "clock.h"
#include "gpio_driver.h"


int main(void){

	SystemClock_Config();

	GPIO_PinConfig_t led ={
			.port = GPIOA,
			.pin	=5,
			.mode	=GPIO_MODE_OUTPUT,
			.otype	=GPIO_OTYPE_PUSHPULL,
			.speed	=GPIO_SPEED_LOW,
			.pull	=GPIO_PULL_NONE
	};

	GPIO_Init(&led);
	/*everything else will be initialized here later*/

	while(1){
		GPIO_TogglePin(GPIOA, 5);
		Delay_ms(500);

		/*Main loop -- will run the state machine later */
	}

}
