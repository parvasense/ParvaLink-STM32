#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "Uart.h"
#include "delay.h"
#include "Lora.h"
#include "debug.h"        // DBG() macro — verbose debug logging, compiles away in lean/production builds

// ---- Shared buffers, used across multiple functions in this file ----
char resp[64];   // holds one line read back from the LoRa module (used in ResponseOK, Lora_CheckRelayCommands)
char json[200];  // holds the built telemetry JSON string (filled by Build_JSON, read by Send_JSON)
char cmd[220];   // holds the full AT+SEND=... command, JSON wrapped inside it (filled by Send_JSON, sent by Send_Command)

// ---- Timing/retry constants, used inside ResponseOK() and Send_Command() ----
#define RESP_TIMEOUT_MS 3000     // total time ResponseOK() will wait for a matching reply before giving up
#define LINE_TIMEOUT_MS 500      // max time Lora_ReadLine() waits for ONE line within that total window

#define CMD_MAX_RETRIES    3     // how many times Send_Command() retries a failed AT command
#define CMD_RETRY_DELAY_MS 500   // pause between retries inside Send_Command()

#define ACK_TIMEOUT_MS 3000      // total wait time used by Send_With_ACK() — currently unused/dead code

#define LORA_DEST_ADDR 1         // home node's LoRa address — used by Send_JSON() when building AT+SEND=<addr>,...

// ---- Relay pin definitions — CONFIRM/REPLACE WITH YOUR ACTUAL WIRING ----
#define START_RELAY   5
#define STOP_RELAY   6
#define RELAY_PULSE_MS 1000

typedef struct
{
	float a;
	float v;

} phase_t;

typedef struct
{
	uint32_t t;

	phase_t p[3];

	float T;
	float H;

	uint8_t m;

}LatestData_t;

LatestData_t Latest_data;

void Load_Data(void)
{
    Latest_data.t = 100;

    Latest_data.p[0].a = 230.5;
    Latest_data.p[0].v = 5.1;

    Latest_data.p[1].a = 231.0;
    Latest_data.p[1].v = 5.2;

    Latest_data.p[2].a = 229.8;
    Latest_data.p[2].v = 5.0;

    Latest_data.T = 32.6;

    Latest_data.H = 68.4;

    Latest_data.m = 0;
}


uint8_t Lora_ReadLine(char *buf, uint8_t max_len, uint32_t timeout_ms)
{
    uint8_t i = 0;
    char c;
    uint32_t start = ms_ticks;

    while (i < max_len - 1)
    {
        uint32_t elapsed = ms_ticks - start;
        if (elapsed >= timeout_ms) break;            // total time bound, not per-char

        c = Lora_ReadChar(timeout_ms - elapsed);      // remaining budget only
        if (c == 0) break;
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}


void Lora_FlushRx(uint32_t flush_ms)
{
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < flush_ms)
    {
        Lora_ReadChar(10);
    }
}


uint8_t ResponseOK(void)
{
    uint32_t start = ms_ticks;

    while ((ms_ticks - start) < RESP_TIMEOUT_MS)
    {
        uint32_t remaining = RESP_TIMEOUT_MS - (ms_ticks - start);
        uint32_t line_to   = (remaining < LINE_TIMEOUT_MS) ? remaining : LINE_TIMEOUT_MS;

        memset(resp, 0, sizeof(resp));

        if (Lora_ReadLine(resp, sizeof(resp), line_to))
        {
            DBG("RX: "); DBG(resp); DBG("\r\n");

            if (strstr(resp, "OK") || strstr(resp, "READY") || strstr(resp, "RESET"))
                return 1;

            if (strstr(resp, "ERR"))
            {
                DBG("AT ERROR\r\n");
                return 0;
            }
        }
    }
    return 0;
}


uint8_t Send_Command(const char *cmd)
{
    for (uint8_t retry = 0; retry < CMD_MAX_RETRIES; retry++)
    {
        // Kept ungated — this is the line that shows the JSON payload
        // (inside the AT+SEND=... command) in the terminal every time.
        Uart2_Debug("TX: ");
        Uart2_Debug(cmd);

        Lora_Send(cmd);

        if (ResponseOK())
        {
        	DBG(" OK\r\n");
        	return 1;
        }

        DBG(" RETRY\r\n");
        delay_ms(CMD_RETRY_DELAY_MS);
    }
    DBG(" FAIL\r\n");
    return 0;
}


uint8_t Build_JSON(void)
{
    int len = snprintf(json, sizeof(json),
        "{\"t\":%lu,"
        "\"p\":["
        "{\"a\":%.1f,\"v\":%.1f},"
        "{\"a\":%.1f,\"v\":%.1f},"
        "{\"a\":%.1f,\"v\":%.1f}"
        "],"
        "\"T\":%.1f,\"H\":%.1f,\"m\":%d}",
        (unsigned long)Latest_data.t,
        Latest_data.p[0].a, Latest_data.p[0].v,
        Latest_data.p[1].a, Latest_data.p[1].v,
        Latest_data.p[2].a, Latest_data.p[2].v,
        Latest_data.T, Latest_data.H, Latest_data.m);

    if (len < 0 || (size_t)len >= sizeof(json))
    {
        Uart2_Debug("JSON TRUNCATED\r\n");   // kept — real error, always show
        return 0;
    }
    return 1;
}


uint8_t Send_JSON(void)
{
    if (!Build_JSON()) return 0;

    int json_len = (int)strlen(json);
    int cmd_len = snprintf(cmd, sizeof(cmd),
        "AT+SEND=%d,%d,%s\r\n", LORA_DEST_ADDR, json_len, json);

    if (cmd_len < 0 || (size_t)cmd_len >= sizeof(cmd))
    {
        Uart2_Debug("CMD TRUNCATED\r\n");     // kept — real error, always show
        return 0;
    }
    return Send_Command(cmd);
}


void Relay_Init(void)
{
	RCC->AHB1ENR |= (1 << 0);

	//PA5
	GPIOA->MODER |=  (1 << 10);
	GPIOA->MODER &= ~(1 << 11);

	//PA6
	GPIOA->MODER |=  (1 << 12);
	GPIOA->MODER &= ~(1 << 13);

	//Initially OFF
	GPIOA->ODR |= (1 << 5);
	GPIOA->ODR |= (1 << 6);
}


// Active-low: 0 = ON, 1 = OFF
void Relay_Pulse(uint8_t pin)
{
	GPIOA->BSRR = (1 << (pin + 16)); // reset bit -> pin LOW -> relay ON
	delay_ms(RELAY_PULSE_MS);
	GPIOA->BSRR = (1 << pin);		// set bit -> pin HIGH -> relay OFF
}


// Sends current Latest_data as JSON to Home Node — doubles as the
// relay confirmation, since Home Node's send_relay_with_confirm()
// just checks the next packet's "m" field, no special ACK tag needed.
static uint8_t Lora_SendAck(void)
{
    return Send_JSON();   // Send_Command's "TX: " line already shows the JSON
}


void Lora_CheckRelayCommands(uint32_t window_ms)
{
    uint32_t start = ms_ticks;

    while ((ms_ticks - start) < window_ms)
    {
        uint32_t remaining = window_ms - (ms_ticks - start);
        uint32_t chunk = (remaining < 200) ? remaining : 200;

        memset(resp, 0, sizeof(resp));

        if (Lora_ReadLine(resp, sizeof(resp), chunk))
        {
            DBG("RX: ");
            DBG(resp);
            DBG("\r\n");

            uint8_t is_rcv = (strstr(resp, "+RCV=") != NULL);  // computed once

            if (is_rcv && strstr(resp, "RELAY1"))
            {
                if (Latest_data.m == 1)
                {
                    DBG("RELAY1: already ON, resending confirm\r\n");
                    Lora_SendAck();
                }
                else
                {
                    DBG("RELAY1 -> Motor ON (pulse)\r\n");
                    Relay_Pulse(START_RELAY);
                    Latest_data.m = 1;
                    Lora_SendAck();
                }
            }
            else if (is_rcv && strstr(resp, "RELAY2"))
            {
                if (Latest_data.m == 0)
                {
                    DBG("RELAY2: already OFF, resending confirm\r\n");
                    Lora_SendAck();
                }
                else
                {
                    DBG("RELAY2 -> Motor OFF (pulse)\r\n");
                    Relay_Pulse(STOP_RELAY);
                    Latest_data.m = 0;
                    Lora_SendAck();
                }
            }
        }
    }
}


void Lora_Init(void)
{
    Uart2_Debug("Initialising LoRa...\r\n");   // kept — one-time boot status
    delay_ms(2000);
    Lora_FlushRx(200);
    Send_Command("AT\r\n");
    Send_Command("AT+RESET\r\n");
    delay_ms(3000);
    Lora_FlushRx(200);
    Send_Command("AT+ADDRESS=2\r\n");
    Send_Command("AT+NETWORKID=5\r\n");
    Send_Command("AT+BAND=865000000\r\n");
    Send_Command("AT+PARAMETER=9,7,1,12\r\n");
    Send_Command("AT+CRFOP=22\r\n");
    Uart2_Debug("LoRa Ready\r\n");              // kept — boot status
}
