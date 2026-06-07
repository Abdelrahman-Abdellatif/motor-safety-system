#ifndef HCSR04_H
#define HCSR04_H

#include "stm32f4xx.h" /* CMSIS — gives you all register names */
#include <stdint.h>

/*----------------------------------------------------------------
 * WHY THIS DRIVER EXISTS:
 *
 * The HC-SR04 is the robot's eyes. It measures distance to
 * obstacles in front of the servo-mounted sensor head.
 *
 * This driver sits on top of two lower drivers:
 *   - gpio_driver  → controls the TRIG output pin
 *   - timer_driver → TIM2 for us delay, TIM4 for echo capture
 *
 * Upper layers (state machine) only call HCSR04_Measure().
 * They never touch GPIO or timers directly for this sensor.
 * That is what layered architecture means.
 *----------------------------------------------------------------*/

/*Maximum measurable distance in cm
 * Beyond this we consider path clear (no obstacle)
 * */
#define HCSR04_MAX_DISTANCE_CM		400.0f
/* Timeout in microsecounds -- if echo does not arrive in 38ms
 *  it means no obstacle detected within range
 * */
#define HCSR04_TIMEOUT_US			38000

/* Initialize TRIG pin (pA0) as GPIO output
 * TIM4 input capture on PB6 is initialized separately
 * in timer_driver - call TIM4_InputCapture_init() before this
 * */
void HCSR04_Init(void);

/*
 * Triggers one measurement and returns distance in cm
 * This function blocks for the duration of the echo pulse
 * Maximum blocking time = HCSR04_TIMEOUT_US = 38ms
 *
 * Returns: distance in cm (2.0 to 400.0)
 *          0.0 if timeout (no obstacle in range)
 */

float HCSR04_Measure(void);


#endif /*HCSR04_H*/
