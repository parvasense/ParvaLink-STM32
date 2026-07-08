#ifndef UART_H_
#define UART_H_

// ─── USART2 — Debug (PA2 TX, PA3 RX) via Pico passthrough
void Uart2_Init(void);
void Uart2_Debug(const char *str);

// ─── USART1 — LoRa RYLR998 (PA9 TX, PA10 RX)
void Uart1_Init(void);
void Lora_Send(const char *str);
char Lora_ReadChar(uint32_t  timeout_ms);
uint8_t Lora_ReadLine(char *buf, uint8_t max_len, uint32_t timeout_ms);

#endif /* UART_H_ */
