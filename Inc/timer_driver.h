#ifndef TIMER_DRIVER_H
#define TIMER_DRIVER_H

#include "stm32f4xx.h"
#include <stdint.h>

/*----------------------------------------------------------------
 * WHY THIS FILE EXISTS:
 *
 * This driver gives every other driver access to timing services.
 * Without precise timing:
 *   - HC-SR04 trigger pulse would be wrong length → bad readings
 *   - Stepper would run at wrong speed → wrong position
 *   - Servo would go to wrong angle → wrong sensor pointing
 *
 * Three services this driver provides:
 *   1. Microsecond delay    → for HC-SR04 trigger pulse
 *   2. PWM output           → for stepper pulses and servo angle
 *   3. Input capture        → for HC-SR04 echo measurement
 *----------------------------------------------------------------*/


/*----------------------------------------------------------------
 * SERVICE 1 — Microsecond timebase (TIM2)
 *
 * TIM2 runs as a free-running counter at 1MHz
 * → each count = exactly 1 microsecond
 * Used for precise short delays and timestamping
 *----------------------------------------------------------------*/

void TIM2_Init(void);
void Delay_us(uint32_t us);
uint32_t TIM2_GetMicros(void); /* return current microsecound count */

/*----------------------------------------------------------------
 * SERVICE 2 — PWM Output (TIM3)
 *
 * TIM3 generates PWM signals on two channels:
 *   CH1 (PB4) → Stepper STEP pulses
 *   CH2 (PB5) → Servo PWM (50Hz, 1000-2000us pulse width)
 *
 * For stepper: frequency controls speed (higher freq = faster)
 * For servo:   pulse width controls angle (1000us=0°, 2000us=180°)
 *----------------------------------------------------------------*/

void TIM3_PWM_Init(void);
void TIM3_SetStepperFrequency(uint32_t freq_hz);  /* set step pulse frequency   */
void TIM3_SetServoPulse(uint16_t pulse_us);        /* set servo pulse 1000-2000us*/
void TIM3_StepperStart(void);                      /* enable stepper PWM output  */
void TIM3_StepperStop(void);                       /* disable stepper PWM output */


/*----------------------------------------------------------------
 * SERVICE 3 — Input Capture (TIM4)
 *
 * TIM4 CH1 (PB6) captures timestamps of rising and falling edges
 * Used to measure exact pulse width of HC-SR04 ECHO signal
 *
 * How it works:
 *   1. ECHO goes HIGH  → timer captures timestamp T1
 *   2. ECHO goes LOW   → timer captures timestamp T2
 *   3. Pulse width = T2 - T1 in microseconds
 *   4. Distance = pulse_width / 58.0 in cm
 *----------------------------------------------------------------*/
void     TIM4_InputCapture_Init(void);
uint32_t TIM4_GetEchoPulseWidth_us(void);  /* returns last measured pulse width */

#endif /* TIMER_DRIVER_H */


