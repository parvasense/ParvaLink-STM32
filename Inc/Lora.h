#ifndef LORA_H_
#define LORA_H_

uint8_t ResponseOK(void);
uint8_t Send_Command(const char *cmd);
uint8_t Send_With_ACK(uint16_t seq_id);
void Lora_Init(void);
void Load_Data(void);
uint8_t Build_JSON(void);
uint8_t Send_JSON(void);
void Lora_FlushRx(uint32_t flush_ms);
void Lora_CheckRelayCommands(uint32_t window_ms);
void Relay_Init(void);
void Relay_Pulse(uint8_t pin);

#endif /* LORA_H_ */
