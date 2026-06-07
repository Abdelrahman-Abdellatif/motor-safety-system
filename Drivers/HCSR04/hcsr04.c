#include "hcsr04.h"
#include "gpio_driver.h"
#include "timer_driver.h"


/*----------------------------------------------------------------
 * HCSR04_Init
 *
 * Only job here: configure PA0 as a push-pull output for TRIG.
 * Start it LOW — TRIG must be LOW between measurements.
 *
 * TIM2 and TIM4 must already be initialized before calling this.
 * The correct init order in main.c is:
 *   1. SystemClock_Config()
 *   2. TIM2_Init()              ← microsecond timebase
 *   3. TIM4_InputCapture_Init() ← echo measurement
 *   4. HCSR04_Init()            ← trig pin setup
 *----------------------------------------------------------------*/
void HCSR04_Init(void){
	GPIO_PinConfig_t trig_pin = {
			.port  = GPIOA,
			.pin   = 0,
			.mode  = GPIO_MODE_OUTPUT,
			.otype = GPIO_OTYPE_PUSHPULL,
			.speed = GPIO_SPEED_HIGH,
			.pull  = GPIO_PULL_NONE
	};
	GPIO_Init(&trig_pin);

	/*To ensure it will start Low*/
	GPIO_WritePin(GPIOA, 0, 0);
}
/*----------------------------------------------------------------
 * HCSR04_Measure
 *
 * Complete measurement sequence:
 *
 *   Step 1 — Pull TRIG LOW for 2us (ensure clean start)
 *   Step 2 — Pull TRIG HIGH for 10us (trigger the sensor)
 *   Step 3 — Pull TRIG LOW (end of trigger pulse)
 *   Step 4 — Wait for TIM4 to capture the echo pulse
 *   Step 5 — Read pulse width from TIM4
 *   Step 6 — Convert to distance using speed of sound formula
 *
 * This function is BLOCKING — it waits for echo to complete.
 * Maximum wait time = HCSR04_TIMEOUT_US (38ms).
 *
 * In the final state machine this will be called periodically
 * from the main loop, not in a tight while(1) — so the 38ms
 * blocking is acceptable and does not freeze the system.
 *----------------------------------------------------------------*/
float HCSR04_Measure(void){
	/*Ensure TRIG is LOW before triggering
	 * Some sensors behave strangely if TRIG was left HIGH
	 * 2us is enough to guarantee a clean LOW state
	 * */
	GPIO_WritePin(GPIOA, 0 ,0);
	Dela_us(2);

	/*
     * Step 2 — Send 10us HIGH pulse on TRIG
     * This is the command that tells the sensor to fire
     * Must be at least 10us — we use 12us for safety margin
     */
	GPIO_WritePin(GPIOA, 0, 1);
	Delay_us(12);
    /*
     * Step 3 — Pull TRIG LOW
     * Sensor is now internally processing and will send
     * the ultrasonic burst automatically
     */
	GPIO_WritePin(GPIOA, 0, 0);

    /*
     * Step 4 — Wait for echo measurement to complete
     *
     * TIM4 interrupt handler (in timer_driver.c) is running
     * in the background. It will:
     *   - Capture rising edge of ECHO → store rise time
     *   - Capture falling edge of ECHO → calculate pulse width
     *   - Set echo_ready flag = 1
     *
     * We poll TIM4_GetEchoPulseWidth_us() which returns 0
     * until echo_ready is set.
     *
     * Timeout prevents infinite blocking if no echo arrives
     * (obstacle out of range, or sensor not connected)
     */
	uint32_t timeout_start = TIM2_GetMicros();
	uint32_t pulse_width   = 0;
	while (1){
		pulse_width = TIM4_GetEchoPulseWidth_us();
		/*Got a measurement*/
		if (pulse_width > 0)
			break;
		/* Timeout — no echo received within 38ms */
		if ((TIM2_GetMicros() - timeout_start) > HCSR04_TIMEOUT_US)
			return 0.0f; /* 0.0 = no obstacle detected */
	}
    /*
     * Step 5 — Convert pulse width to distance
     *
     * Speed of sound = 343 m/s = 0.0343 cm/us
     * Sound travels TO obstacle and BACK (multiply by 2)
     * distance = pulse_us × 0.0343 / 2
     *          = pulse_us / 58.3
     *
     * We use 58.0f as the divisor — standard value for HC-SR04
     */
	float distance_cm = (float)pulse_width / 58.0f;
    /*
     * Step 6 — Clamp result to valid range
     *
     * HC-SR04 minimum reliable distance = 2cm
     * Maximum reliable distance = 400cm
     * Anything outside this range is sensor noise
     */
    if (distance_cm < 2.0f)   return 2.0f;
    if (distance_cm > 400.0f) return 400.0f;

    return distance_cm;

}



