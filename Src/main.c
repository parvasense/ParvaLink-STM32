#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>
#include "Uart.h"
#include "delay.h"
#include "Lora.h"

// Telemetry send interval — matches Pico SEND_INTERVAL_MS
#define SEND_INTERVAL_MS 5000

#define RELAY_CHECK_WINDOW_MS 800

int main(void)
{
	SCB->CPACR |= (0xF << 20);

    SysTick_Config(16000);

    Uart1_Init();

    Uart2_Init();

    delay_ms(1000);

    Relay_Init();

    Uart2_Debug("START\r\n");

    Load_Data();

    Lora_Init();

    uint32_t last_send = ms_ticks;

    while(1)
    {
        Lora_CheckRelayCommands(RELAY_CHECK_WINDOW_MS);

        if ((ms_ticks - last_send) >= SEND_INTERVAL_MS)
        {
            if (Send_JSON())
            {
                Uart2_Debug("JSON SENT\r\n");
            }
            else
            {
                Uart2_Debug("SEND FAILED\r\n");
            }
            last_send = ms_ticks;
        }
    }
}
