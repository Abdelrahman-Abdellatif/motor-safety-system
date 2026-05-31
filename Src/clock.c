#include "clock.h"

/* This variable is incremented every 1ms by SysTick interrupt */
static volatile uint32_t sys_tick_count = 0; // global variable

void SystemClock_Config(void){

	/*
	 * STEP A - Enable HSI and wait until it is ready
	 * RCC->CR bit 0 = HSION (trun on HSI)
	 * RCC->CR bit 1 = HSIRDY (1 = HSI is stable and ready)
	 * */
	RCC->CR |= RCC_CR_HSION;
	while (!(RCC->CR & RCC_CR_HSIRDY));

	/*
	 * STEP B - Configure the PLL
	 * we write the whole PLLCFGR register at once
	 *
	 * PLLSRC = 0	--> use HSI as PLL source
	 * PLLM = 16 	--> divide HSI by 16 -> 1MHz into PLL
	 * PLLN	= 336	--> multiply by 336 -> 336MHz VCO
	 * PLLP = 0b00	--> divide by 2 (00=div2, 01=div4) -> wait
	 *
	 * Actually PLLP bits: 00=2, 01=4, 10=6, 11=8
	 * we want divide by 4 to get 84MHz -> PLLP=0b01
	 *
	 * Final: 336 / 4 = 84MHz
	 *
	 * PLLQ = 4 -> for USB (not used here but must be set correctly)
	 * */

	RCC->PLLCFGR = (16 << RCC_PLLCFGR_PLLM_Pos) |
				   (336 << RCC_PLLCFGR_PLLN_Pos) |
				   (0x01 << RCC_PLLCFGR_PLLP_Pos) |
				   (0 << RCC_PLLCFGR_PLLSRC_Pos) |
				   (4 << RCC_PLLCFGR_PLLQ_Pos);

	/*
	 * STEP C - Enable the PLL and wait until it locks
	 * RCC->CR PLLON bit turns it on
	 * RCC->CR PLLRDY bit = 1 when PLL is locked and stable
	 * */
	RCC->CR |= RCC_CR_PLLON;
	while(!(RCC->CR & RCC_CR_PLLRDY));

	/*
	 * STEP D - COnfigure Flash latency
	 * At 84MHz we need 2 wait states - this is a hardware requirement
	 * If you skip this the CPU reads flash too fast and get garbage
	 * FLASH->ACR is the flash access control register
	 * Also enable instruction cache, data cashe, prefetch
	 * */
	FLASH->ACR = FLASH_ACR_LATENCY_2WS	|
				 FLASH_ACR_ICEN			|
				 FLASH_ACR_DCEN			|
				 FLASH_ACR_PRFTEN;


	/*
	 * STEP E - Set bus prescalers
	 * AHB = SYSCLK / 1 =84MHz  (HPRE = 0 means divide by 1)
	 * APB1 = SYSCLK / 2 = 42 MHz (PPRE1 = 4 means divide by 2)
	 * 			APB1 max is 42 - this is a hardware limit
	 * APB2 = SYSCLK /1 84MHz (PPRE2 = 0 means divide by 1)
	 * */
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1		|
				 RCC_CFGR_PPRE1_DIV2	|
				 RCC_CFGR_PPRE2_DIV1;

	/*
	 * STEP F switch SYSCLK source to PLL
	 * SW bits in RCC->CFGR: 00=HSI, 01=HSE, 10=PLL
	 * After writing, read SWS bits to confirm the switch happened
	 * */
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

	/*
	 * STEP G - Configure SysTick for 1ms interrupt
	 * SysTick is a 24-bit countdown timer built into the Cortex-M4
	 * we want it to fire every 1ms
	 * At 84MHz: counts per ms = 84 , 000, 000 / 1000 = 84, 000
	 * SysTick_Config() is a CMSIS function that does this for us
	 * */
	SysTick_Config(84000);
}

/*
 * SysTick_Handler — this runs every 1ms automatically
 * The name must be exactly this — it overrides the weak handler
 * in the startup file
 */

void SysTick_Handler(void){
	sys_tick_count++;
}
uint32_t GetTick(void){
	return sys_tick_count;
}

void Delay_ms(uint32_t ms){
	uint32_t start = GetTick();
	while ((GetTick() - start) < ms);
}

