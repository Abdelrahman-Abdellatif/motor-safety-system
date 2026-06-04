#include "timer_driver.h"
#include "gpio_driver.h"

/*================================================================
 * PRIVATE VARIABLES
 *
 * These are only visible inside this file (static)
 * Used by the input capture interrupt to store timestamps
 *================================================================*/

/* Stores the microsecond timestamp when ECHO went HIGH */
static volatile uint32_t echo_rise_time = 0;

/* Stores the measured pulse width after ECHO goes LOW */
static volatile uint32_t echo_pulse_width = 0;

/* Flag: 1 = new measurement is ready to be read */
static volatile uint8_t echo_ready = 0;


/*================================================================
 * SERVICE 1 — TIM2 Microsecond Timebase
 *================================================================*/

/*----------------------------------------------------------------
 * TIM2_Init
 *
 * Configures TIM2 as a free-running 1MHz counter.
 * "Free-running" means it counts forever, never stops.
 * We just read CNT register to get current microseconds.
 *
 * TIM2 is on APB1 bus.
 * APB1 timer clock = APB1 × 2 = 42MHz × 2 = 84MHz
 * (STM32 rule: if APB prescaler != 1, timer clock is doubled)
 *
 * To get 1MHz from 84MHz:
 * PSC = 84 - 1 = 83
 * Timer clock = 84MHz / (83+1) = 1MHz ✓
 *
 * ARR = 0xFFFFFFFF (max value — TIM2 is 32-bit)
 * This means it takes 4294 seconds to overflow. Effectively never.
 *----------------------------------------------------------------*/
void TIM2_Init(void)
{
    /* Step 1 — Enable TIM2 clock on APB1 bus */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR; /* small delay after clock enable */

    /* Step 2 — Set prescaler: 84MHz / 84 = 1MHz */
    TIM2->PSC = 84 - 1;

    /* Step 3 — Set auto-reload to maximum (32-bit timer) */
    TIM2->ARR = 0xFFFFFFFF;

    /* Step 4 — Reset counter to 0 */
    TIM2->CNT = 0;

    /* Step 5 — Enable the timer
     * CEN = Counter ENable bit in CR1 register */
    TIM2->CR1 |= TIM_CR1_CEN;
}


/*----------------------------------------------------------------
 * Delay_us
 *
 * Blocking delay in microseconds using TIM2 counter.
 * Reads current count, waits until 'us' ticks have passed.
 *
 * Why not use Delay_ms from SysTick for short delays?
 * SysTick fires every 1ms = 1000us minimum resolution.
 * HC-SR04 needs a 10us pulse — SysTick cannot do that.
 * TIM2 at 1MHz gives 1us resolution — perfect.
 *----------------------------------------------------------------*/
void Delay_us(uint32_t us)
{
    uint32_t start = TIM2->CNT;
    while ((TIM2->CNT - start) < us);
}


/*----------------------------------------------------------------
 * TIM2_GetMicros
 *
 * Returns current microsecond timestamp.
 * Useful for timestamping events and measuring elapsed time.
 *----------------------------------------------------------------*/
uint32_t TIM2_GetMicros(void)
{
    return TIM2->CNT;
}


/*================================================================
 * SERVICE 2 — TIM3 PWM Output
 *================================================================*/

/*----------------------------------------------------------------
 * TIM3_PWM_Init
 *
 * Configures TIM3 for two PWM outputs:
 *   CH1 on PB4 → Stepper STEP pulses
 *   CH2 on PB5 → Servo signal (50Hz)
 *
 * PWM Mode 1: pin goes HIGH at start of period,
 *             goes LOW when counter reaches CCR value.
 *
 *   Period looks like this:
 *   |‾‾‾‾|____|‾‾‾‾|____|
 *   ← CCR →← ARR-CCR →
 *
 * For servo we need:
 *   Frequency = 50Hz → period = 20ms
 *   Pulse width = 1000-2000us (controls angle)
 *
 * Timer setup for servo:
 *   We want 1us resolution for pulse width control
 *   PSC = 84-1 → 1MHz timer clock → 1us per tick
 *   ARR = 20000-1 → period = 20000us = 20ms = 50Hz ✓
 *   CCR2 = 1000 to 2000 → pulse width in microseconds
 *
 * For stepper we use same timer but different ARR per speed.
 * TIM3_SetStepperFrequency() recalculates ARR dynamically.
 *----------------------------------------------------------------*/
void TIM3_PWM_Init(void)
{
    /* Step 1 — Enable TIM3 clock */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;

    /* Step 2 — Configure PB4 (CH1) and PB5 (CH2) as AF2 (TIM3)
     *
     * AF2 = TIM3/TIM4 for PB4 and PB5
     * Check STM32F401 datasheet Table 9 to verify this
     */
    GPIO_PinConfig_t pwm_ch1 = {
        .port  = GPIOB,
        .pin   = 4,
        .mode  = GPIO_MODE_AF,
        .otype = GPIO_OTYPE_PUSHPULL,
        .speed = GPIO_SPEED_HIGH,
        .pull  = GPIO_PULL_NONE
    };
    GPIO_Init(&pwm_ch1);

    GPIO_PinConfig_t pwm_ch2 = {
        .port  = GPIOB,
        .pin   = 5,
        .mode  = GPIO_MODE_AF,
        .otype = GPIO_OTYPE_PUSHPULL,
        .speed = GPIO_SPEED_HIGH,
        .pull  = GPIO_PULL_NONE
    };
    GPIO_Init(&pwm_ch2);

    /* Set AF2 for PB4 and PB5
     * PB4 and PB5 are in AFR[0] (pins 0-7)
     * bit position = pin * 4
     */
    GPIOB->AFR[0] &= ~(0xFUL << (4 * 4));
    GPIOB->AFR[0] |=  (0x2UL << (4 * 4));  /* AF2 for PB4 */

    GPIOB->AFR[0] &= ~(0xFUL << (5 * 4));
    GPIOB->AFR[0] |=  (0x2UL << (5 * 4));  /* AF2 for PB5 */

    /* Step 3 — Set prescaler to get 1MHz timer clock
     * 84MHz / (83+1) = 1MHz → 1 tick = 1us */
    TIM3->PSC = 84 - 1;

    /* Step 4 — Set ARR for 50Hz servo signal
     * Period = 20ms = 20000us
     * ARR = 20000 - 1 */
    TIM3->ARR = 20000 - 1;

    /* Step 5 — Configure CH1 (stepper) in PWM Mode 1
     * CCMR1 controls CH1 and CH2
     * OC1M bits [6:4] = 110 → PWM Mode 1
     * OC1PE = preload enable (ARR updates take effect at next period)
     */
    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1);  /* PWM mode 1 */
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE;                          /* preload    */

    /* Step 6 — Configure CH2 (servo) in PWM Mode 1
     * OC2M bits [14:12] in CCMR1
     */
    TIM3->CCMR1 |= (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1);  /* PWM mode 1 */
    TIM3->CCMR1 |= TIM_CCMR1_OC2PE;                          /* preload    */

    /* Step 7 — Enable CH1 and CH2 outputs
     * CCER register: CC1E = CH1 output enable, CC2E = CH2 output enable
     */
    TIM3->CCER |= TIM_CCER_CC1E;   /* enable CH1 output */
    TIM3->CCER |= TIM_CCER_CC2E;   /* enable CH2 output */

    /* Step 8 — Set initial CCR values
     * CH1 (stepper): 0 → no pulse yet, stepper starts stopped
     * CH2 (servo): 1500us → 90 degrees, center position
     */
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 1500;

    /* Step 9 — Enable auto-reload preload */
    TIM3->CR1 |= TIM_CR1_ARPE;

    /* Step 10 — Start the timer */
    TIM3->CR1 |= TIM_CR1_CEN;
}


/*----------------------------------------------------------------
 * TIM3_SetStepperFrequency
 *
 * Changes how fast the stepper motor moves by changing
 * the PWM frequency on CH1.
 *
 * More pulses per second = more steps per second = faster motor.
 *
 * We keep 50% duty cycle (CCR1 = ARR/2) — the A4988/DRV8825
 * triggers on the rising edge so duty cycle does not matter much,
 * but 50% is clean and standard.
 *
 * Formula:
 *   ARR = (timer_clock / freq) - 1
 *   timer_clock = 1MHz (after PSC)
 *   freq_hz = steps per second
 *
 * Example: 500 steps/sec → ARR = 1000000/500 - 1 = 1999
 *----------------------------------------------------------------*/
void TIM3_SetStepperFrequency(uint32_t freq_hz)
{
    if (freq_hz == 0) return;

    uint32_t arr = (1000000UL / freq_hz) - 1;
    TIM3->ARR  = arr;
    TIM3->CCR1 = arr / 2;   /* 50% duty cycle */
}


/*----------------------------------------------------------------
 * TIM3_SetServoPulse
 *
 * Sets servo angle by changing pulse width on CH2.
 *
 * Standard servo signal:
 *   1000us pulse → 0 degrees   (fully left)
 *   1500us pulse → 90 degrees  (center)
 *   2000us pulse → 180 degrees (fully right)
 *
 * Since our timer runs at 1MHz, CCR2 value = pulse width in us
 *----------------------------------------------------------------*/
void TIM3_SetServoPulse(uint16_t pulse_us)
{
    /* Clamp to safe range — going outside damages the servo */
    if (pulse_us < 500)  pulse_us = 500;
    if (pulse_us > 2500) pulse_us = 2500;

    TIM3->CCR2 = pulse_us;
}


/*----------------------------------------------------------------
 * TIM3_StepperStart / Stop
 *
 * Enables or disables CH1 PWM output.
 * When stopped, the STEP pin goes LOW — DRV8825 stops pulsing.
 * The motor holds its position (DRV8825 keeps coils energized).
 *----------------------------------------------------------------*/
void TIM3_StepperStart(void)
{
    TIM3->CCER |= TIM_CCER_CC1E;
}

void TIM3_StepperStop(void)
{
    TIM3->CCER &= ~TIM_CCER_CC1E;
    GPIO_WritePin(GPIOB, 4, 0);   /* force STEP pin LOW */
}


/*================================================================
 * SERVICE 3 — TIM4 Input Capture
 *================================================================*/

/*----------------------------------------------------------------
 * TIM4_InputCapture_Init
 *
 * Configures TIM4 CH1 on PB6 to capture both edges of HC-SR04
 * ECHO pulse.
 *
 * How input capture works:
 *   - You configure a pin as input capture channel
 *   - When the pin changes state (rising or falling edge)
 *     the timer automatically copies CNT into CCR1
 *   - This gives you an exact timestamp of when the edge happened
 *   - Subtract rise timestamp from fall timestamp = pulse width
 *
 * We use interrupt to detect each edge and store timestamps.
 *----------------------------------------------------------------*/
void TIM4_InputCapture_Init(void)
{
    /* Step 1 — Enable TIM4 clock */
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    (void)RCC->APB1ENR;

    /* Step 2 — Configure PB6 as AF2 (TIM4 CH1) */
    GPIO_PinConfig_t echo_pin = {
        .port  = GPIOB,
        .pin   = 6,
        .mode  = GPIO_MODE_AF,
        .otype = GPIO_OTYPE_PUSHPULL,
        .speed = GPIO_SPEED_HIGH,
        .pull  = GPIO_PULL_DOWN   /* pull down: line idle = LOW */
    };
    GPIO_Init(&echo_pin);

    /* PB6 → AF2 for TIM4
     * PB6 is in AFR[0] (pins 0-7), position = 6*4 = 24 */
    GPIOB->AFR[0] &= ~(0xFUL << (6 * 4));
    GPIOB->AFR[0] |=  (0x2UL << (6 * 4));

    /* Step 3 — Set prescaler: 1MHz (same as TIM2 for consistency) */
    TIM4->PSC = 84 - 1;

    /* Step 4 — Set ARR to maximum (16-bit timer) */
    TIM4->ARR = 0xFFFF;

    /* Step 5 — Configure CH1 as input capture
     * CCMR1 CC1S bits = 01 → CH1 is input, mapped to TI1
     * This means TIM4_CH1 pin (PB6) is the input source
     */
    TIM4->CCMR1 &= ~TIM_CCMR1_CC1S;
    TIM4->CCMR1 |=  TIM_CCMR1_CC1S_0;   /* CC1S = 01 → input on TI1 */

    /* Step 6 — Capture on rising edge first
     * CCER CC1P = 0 → rising edge
     * CCER CC1NP = 0 → (must be 0 for input capture)
     */
    TIM4->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP);

    /* Step 7 — Enable capture interrupt
     * CC1IE = Capture Compare 1 Interrupt Enable
     */
    TIM4->DIER |= TIM_DIER_CC1IE;

    /* Step 8 — Enable TIM4 interrupt in NVIC
     * Lower priority than UART (priority 1 vs UART priority 0)
     */
    NVIC_SetPriority(TIM4_IRQn, 1);
    NVIC_EnableIRQ(TIM4_IRQn);

    /* Step 9 — Enable CH1 capture and start timer */
    TIM4->CCER |= TIM_CCER_CC1E;
    TIM4->CR1  |= TIM_CR1_CEN;
}


/*----------------------------------------------------------------
 * TIM4_IRQHandler
 *
 * Fires on every edge of the ECHO pin (PB6).
 *
 * First call  → rising edge  → save timestamp as rise time
 *                               reconfigure to catch falling edge
 * Second call → falling edge → calculate width = now - rise
 *                               reconfigure to catch rising edge
 *                               set echo_ready flag
 *
 * This alternating edge detection is the standard way to measure
 * pulse width with input capture on STM32.
 *----------------------------------------------------------------*/
void TIM4_IRQHandler(void)
{
    /* Confirm interrupt source is CH1 capture event */
    if (TIM4->SR & TIM_SR_CC1IF)
    {
        /* Clear the interrupt flag by reading CCR1
         * Reading CCR1 automatically clears CC1IF */
        uint32_t captured = TIM4->CCR1;

        if (!(TIM4->CCER & TIM_CCER_CC1P))
        {
            /* Currently configured for rising edge → this IS the rise */
            echo_rise_time = captured;

            /* Reconfigure to catch falling edge next */
            TIM4->CCER |= TIM_CCER_CC1P;
        }
        else
        {
            /* Currently configured for falling edge → this IS the fall */
            echo_pulse_width = captured - echo_rise_time;
            echo_ready = 1;

            /* Reconfigure to catch rising edge next (for next measurement) */
            TIM4->CCER &= ~TIM_CCER_CC1P;
        }
    }
}


/*----------------------------------------------------------------
 * TIM4_GetEchoPulseWidth_us
 *
 * Returns the last measured ECHO pulse width in microseconds.
 * Returns 0 if no measurement is ready yet.
 *
 * The HC-SR04 driver calls this after sending a trigger pulse
 * and waiting enough time for the echo to return.
 *----------------------------------------------------------------*/
uint32_t TIM4_GetEchoPulseWidth_us(void)
{
    if (!echo_ready) return 0;
    echo_ready = 0;
    return echo_pulse_width;
}
