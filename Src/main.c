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

#include "stm32f4xx.h"
#include "clock.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "timer_driver.h"

int main(void)
{
    SystemClock_Config();
    UART_Init(115200);
    TIM2_Init();
    TIM3_PWM_Init();
    TIM4_InputCapture_Init();

    UART_SendString("Timer driver initialized\r\n");

    /* Sweep servo from 0 to 180 degrees and back */
    while (1)
    {
        /* Sweep left to right: 1000us to 2000us */
        for (uint16_t pulse = 1000; pulse <= 2000; pulse += 10)
        {
            TIM3_SetServoPulse(pulse);
            Delay_ms(20);
        }

        /* Sweep right to left: 2000us to 1000us */
        for (uint16_t pulse = 2000; pulse >= 1000; pulse -= 10)
        {
            TIM3_SetServoPulse(pulse);
            Delay_ms(20);
        }

        UART_SendString("Servo sweep complete\r\n");
    }
}
