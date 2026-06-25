#include <stm32f4xx.h>
#include "delay.h"

volatile uint32_t ms_ticks = 0;

void SysTick_Handler(void)
{
	ms_ticks++;
}

void delay_ms(uint32_t ms)
{
	uint32_t start = ms_ticks;
	while((ms_ticks - start) < ms);
}

