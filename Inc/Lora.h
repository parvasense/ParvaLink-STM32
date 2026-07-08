#ifndef LORA_H_
#define LORA_H_

#include <stdint.h>   // uint8_t, uint16_t, uint32_t — needed for the types used in prototypes

void     Load_Data(void);

uint8_t  Lora_ReadLine(char *buf, uint8_t max_len, uint32_t timeout_ms);
void     Lora_FlushRx(uint32_t flush_ms);
uint8_t  ResponseOK(void);
uint8_t  Send_Command(const char *cmd);

uint8_t  Build_JSON(void);
uint8_t  Send_JSON(void);

void     Relay_Init(void);
void     Relay_Pulse(uint8_t pin);
void     Lora_CheckRelayCommands(uint32_t window_ms);

void     Lora_Init(void);

#endif /* LORA_H_ */
