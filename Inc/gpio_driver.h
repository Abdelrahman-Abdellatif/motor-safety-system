#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include "stm32f4xx.h" /* CMSIS — gives you all register names */
#include <stdint.h>

/*---------------------------------------------------------------------------
 *  Enumerations - these make the API readable and safe
 *--------------------------------------------------------------------------- */

/*Creating The pin mode options*/
typedef enum {
	GPIO_MODE_INPUT 	= 0x00,
	GPIO_MODE_OUTPUT	= 0x01,
	GPIO_MODE_AF 		= 0x02,
	GPIO_MODE_ANALOG 	= 0x03
} GPIO_Mode_t;

/*Creating the output type of the pins*/
typedef enum {
	GPIO_OTYPE_PUSHPULL = 0x00, /*default for most outputs*/
	GPIO_OTYPE_OPENDRAIN = 0x01
}GPIO_OType_t;

typedef enum{
	GPIO_SPEED_LOW		= 0x00,
	GPIO_SPEED_MEDIUM	= 0x01,
	GPIO_SPEED_HIGH		= 0x02,
	GPIO_SPEED_VHIGH	= 0x03
} GPIO_Speed_t;

typedef enum {
	GPIO_PULL_NONE	= 0x00,
	GPIO_PULL_UP	= 0x01,
	GPIO_PULL_DOWN	= 0x02
}GPIO_Pull_t;

/*
 * Config struct -- one structure describe one pin completely
 * */
typedef struct {
	GPIO_TypeDef *port; /*pointer to port GPIOA, GPIOB, ...*/
	uint8_t		  pin; /*0 to 15*/
	GPIO_Mode_t	  mode;
	GPIO_OType_t  otype;
	GPIO_Speed_t  speed;
	GPIO_Pull_t   pull;
}GPIO_PinConfig_t;

/* ===========================================================
 * Function declarations
 * ===========================================================*/

void	GPIO_ClockEnable(GPIO_TypeDef *port);
void 	GPIO_Init(const GPIO_PinConfig_t *config);
void 	GPIO_WritePin(GPIO_TypeDef *port, uint8_t pin, uint8_t state);
uint8_t GPIO_ReadPin(GPIO_TypeDef *port, uint8_t pin);
void 	GPIO_TogglePin(GPIO_TypeDef *port, uint8_t pin);


#endif /* GPIO_DRIVER_H */


