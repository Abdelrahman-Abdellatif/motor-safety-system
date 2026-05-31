#ifndef CLOCK_H
#define CLOCK_H

#include "stm32f4xx.h"

/*
 * Initialize system clock to 84MGz using HSI + PPL
 * Must be called first thing in main() before anything else
 */
void SystemClock_Config(void);

/*
 * Return a simple tick counter incremented by SysTick
 * Used for non-blocking delays
 * */
uint32_t GetTick(void);

/*
 *  Blocking delay in millisecounds
 * */
void Delay_ms(uint32_t ms);

#endif /*CLOCK_H*/
