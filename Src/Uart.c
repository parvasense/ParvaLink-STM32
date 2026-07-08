#include "stm32f4xx.h"
#include "Uart.h"
#include <stdint.h>
#include "delay.h"

// ─── USART1 RX ring buffer (ISR-fed, fixes dropped bytes) ───
#define LORA_RX_BUF_SIZE 128

static volatile uint8_t  lora_rx_buf[LORA_RX_BUF_SIZE];
static volatile uint16_t lora_rx_write = 0;   // written only by ISR (where new bytes go in)
static volatile uint16_t lora_rx_read  = 0;   // written only by main code (where we read out)

static inline uint8_t Lora_RxAvailable(void)
{
    return (lora_rx_write != lora_rx_read);
}

void USART1_IRQHandler(void)
{
    if (USART1->SR & (1 << 5))          // RXNE
    {
        uint8_t byte = (uint8_t)USART1->DR;   // reading DR clears RXNE/ORE
        uint16_t next_write = (lora_rx_write + 1) % LORA_RX_BUF_SIZE;

        if (next_write != lora_rx_read)  // buffer not full
        {
            lora_rx_buf[lora_rx_write] = byte;
            lora_rx_write = next_write;
        }
        // else: buffer full -> byte dropped (better than blocking the ISR)
    }

    if (USART1->SR & (1 << 3))          // ORE (overrun)
    {
        (void)USART1->DR;                // clear it
    }
}

// ─── USART2 — Debug (PA2 TX, PA3 RX) via Pico passthrough
void Uart2_Init(void)
{
	//Enable the clock access for GPIOA
	RCC->AHB1ENR |= (1 << 0);
	//Enable the clock access for USART2
	RCC->APB1ENR |= (1 << 17);
	//Set ALT FUN for PA2 TX & PA3 RX
    GPIOA->MODER &= ~((3 << 4) | (3 << 6));
    GPIOA->MODER |=  ((2 << 4) | (2 << 6));
	//Configure AFLR AF7 -> 0111
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
    GPIOA->AFR[0] |=  ((7 << 8) | (7 << 12));
	//configure the baud rate
	USART2->BRR = 0x0683;
	//Configure the USART2 EN, TE & RE
	USART2->CR1 |= ((1 << 13) | (1 << 2) | (1 << 3));
}

void Uart2_Debug(const char *str)
{
	while(*str)
	{
		while(!(USART2->SR & (1 << 7)));
		USART2->DR = (uint8_t)*str++;
	}
}

// ─── USART1 — LoRa RYLR998 (PA9 TX, PA10 RX)
void Uart1_Init(void)
{
	//Enable the clock access for GPIOA
	RCC->AHB1ENR |= (1 << 0);
	//Enable the clock access for USART1
	RCC->APB2ENR |= (1 << 4);
	//Set ALT FUN for PA9 TX & PA10 RX
    GPIOA->MODER &= ~((3 << 18) | (3 << 20));
    GPIOA->MODER |=  ((2 << 18) | (2 << 20));
	//Configure AFLR AF2 -> 0111
    GPIOA->AFR[1] &= ~((0xF << 4) | (0xF << 8));
    GPIOA->AFR[1] |=  ((7 << 4) | (7 << 8));
	//configure the baud rate
    USART1->BRR = 0x008B;   // 115200 baud @ 16MHz HSI
	//Configure the USART1 EN, TE & RE
	USART1->CR1 |= ((1 << 13) | (1 << 2) | (1 << 3));

	// NEW: enable RXNE interrupt so no byte is ever missed
	USART1->CR1 |= (1 << 5);            // RXNEIE
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_EnableIRQ(USART1_IRQn);
}

void Lora_Send(const char *str)
{
	while(*str)
	{
		while(!(USART1->SR & (1 << 7))){};
		USART1->DR = (uint8_t)*str++;
	}
}

char Lora_ReadChar(uint32_t timeout_ms)
{
	uint32_t start = ms_ticks;

	while(!Lora_RxAvailable())
	{
		if((ms_ticks - start) >= timeout_ms)
		{
			return 0;
		}
	}

	uint8_t byte = lora_rx_buf[lora_rx_read];
	lora_rx_read = (lora_rx_read + 1) % LORA_RX_BUF_SIZE;
	return (char)byte;
}
