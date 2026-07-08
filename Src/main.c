#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>
#include "Uart.h"
#include "delay.h"
#include "Lora.h"

// Slow telemetry — relay responsiveness is the priority now
#define SEND_INTERVAL_MS      30000

// Very tight relay-check slice — near-instant reaction to incoming commands
#define RELAY_CHECK_WINDOW_MS 100

int main(void)
{
    SCB->CPACR |= (0xF << 20);   // enable FPU — required for float snprintf

    SysTick_Config(16000);

    Uart1_Init();
    Uart2_Init();

    delay_ms(1000);

    Relay_Init();

    Uart2_Debug("START\r\n");

    Load_Data();

    Lora_Init();

    uint32_t last_send = ms_ticks;

    while (1)
    {
        // Check for relay commands first — highest priority, every 100ms
        Lora_CheckRelayCommands(RELAY_CHECK_WINDOW_MS);

        // Telemetry: best-effort, slow, never allowed to starve relay checks
        if ((ms_ticks - last_send) >= SEND_INTERVAL_MS)
        {
            if (Send_JSON())
                Uart2_Debug("JSON SENT\r\n");
            else
                Uart2_Debug("SEND FAILED\r\n");

            last_send = ms_ticks;
        }
    }
}
