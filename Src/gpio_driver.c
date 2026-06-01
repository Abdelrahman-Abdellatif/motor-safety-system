#include "gpio_driver.h"

/* =======================================================================
 * GPIO_CLockEnable
 * Before we work with any GPIO register we need to enable its clock in RCC
 * RCC->AHB1ENR controls GPIO clocks - each bit = one port
 * */
void GPIO_ClockEnable(GPIO_TypeDef *port){
	if		(port == GPIOA) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	else if (port == GPIOB) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	else if (port == GPIOC) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	else if (port == GPIOD) RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
	else if (port == GPIOE) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
}

/* ===================================================================
 *
 * GPIO_Init
 *
 * Configures a single pin using the config struct
 * Order matters here:
 * 1. Enable clock first
 * 2. set mode
 * 3. set output type
 * 4. set speed
 * 5. set pull up/down
 *
 * ===================================================================*/

void GPIO_Init(const GPIO_PinConfig_t *config){
	/*1 ) Enable the peripheral clock for this port */
	GPIO_ClockEnable(config->port);

	/*2) set pin mode in MODER register*
	 * MODER has 2 bits per pin --> position = pin * 2
	 * first clear the 3 bits with ~(0x3 << pin*2)
	 * then we write the new mode value
	 * */
	/*config is a pointer, of type GPIO_PinConfig
	 * the port is also a pointer of type GPIO_TypeDef
	 * that is why we point to a pointer that points to the MODER register
	 * pin is a variable of type unsigned integer 8
	 * */
	config->port->MODER &= ~(0x3UL << (config->pin *2));
	config->port->MODER |=  (config->mode << (config->pin *2));

	/* =================================================================
	 * step 3 - set output type in OTYPER register
	 *
	 * OTYPER has 1 bit per pin -> position = pin
	 * 0 = push-pull, 1 = open-drain
	 * =================================================================*/
	config->port->OTYPER &= ~(0x1UL << config->pin);
	config->port->OTYPER |=  (config->otype << config->pin);

	/*==================================================================
	 * Set output speed in OSPEEDR register
	 * =================================================================*/
	config->port->OSPEEDR &= ~(0x3UL << (config->pin *2));
	config->port->OSPEEDR |=  (config->speed << (config->pin *2));

	/* =======================================================
	 * STEP 5 set pull-up / pull-down in PUPDR register
	 * =====================================================*/
	config->port->PUPDR &= ~(0x3UL << (config->pin *2));
	config->port->PUPDR |= (config->pull << (config->pin *2));
}


/* +==========================================================================
 * GPIO_WriitePin
 * Uses BSRR register
 * BSRR is atomic - no read-modify-write needed
 *
 * Lower 16 bits = SET the pin (write 1 to bit N -> pin N goes high)
 * Upper 16 bits = RESET the pin (write 1 to bit N+16 -> pin N goes LOW)
 *
 * it is better to use the BSRR register instead of the ODR becouse ODR requires 3 steps to write the bit [not atomic]
 * on the other had the BSRR write it in 1 instruction [atomic]
 * In an interrupt-driven system that causes race conditions.
 * BSRR is always the corrtect way
 * */

void GPIO_WritePin(GPIO_TypeDef *port, uint8_t pin, uint8_t state){

	if (state)
		port->BSRR = (1UL << pin);  /*	 SET	*/
	else
		port->BSRR = (1UL << (pin + 16)); /* 	RESET	*/
}

/* ==========================================================================
 * GPIO_ReadPin
 * READS IDR - input data register
 * shift the target bit down to position 0, mast with 1
 * returns 0 or 1
 * ========================================================================== */

uint8_t GPIO_ReadPin(GPIO_TypeDef *port, uint8_t pin){
	return (uint8_t)((port->IDR >> pin) & 0x1UL);
}

/* ==========================================================================
 * GPIO_TogglePin
 * ========================================================================== */
void GPIO_TogglePin(GPIO_TypeDef *port, uint8_t pin){
	port->ODR ^= (1UL <<pin);
}






