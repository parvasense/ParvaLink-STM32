#ifndef DELAY_H_
#define DELAY_H_

void SysTick_Handler(void);
void delay_ms(uint32_t ms);

extern volatile uint32_t ms_ticks;

#endif /* DELAY_H_ */
