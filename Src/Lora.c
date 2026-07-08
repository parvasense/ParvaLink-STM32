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
#define RESP_TIMEOUT_MS 1000     // total time ResponseOK() will wait for a matching reply before giving up
#define LINE_TIMEOUT_MS 300      // max time Lora_ReadLine() waits for ONE line within that total window

#define CMD_MAX_RETRIES    1     // how many times Send_Command() retries a failed AT command
#define CMD_RETRY_DELAY_MS 200   // pause between retries inside Send_Command()

#define ACK_TIMEOUT_MS 3000      // total wait time used by Send_With_ACK() — currently unused/dead code

#define LORA_DEST_ADDR 1         // home node's LoRa address — used by Send_JSON() when building AT+SEND=<addr>,...

// ---- Relay pin definitions — CONFIRM/REPLACE WITH YOUR ACTUAL WIRING ----
#define START_RELAY   5
#define STOP_RELAY   6
#define RELAY_PULSE_MS 1000

// ---- Command ID dedup — core of the new protocol ----
// Initialized to 0xFFFF — a value the home node never generates
// (next_cmd_id starts at 1, wraps at 65535), so the very first
// real command is never mistaken for a duplicate on cold boot.
#define INVALID_CMD_ID 0xFFFF
static uint16_t last_cmd_id = INVALID_CMD_ID;

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

// NEW: parses "CMD,<id>,<action>" out of a +RCV= line.
// Returns 1 on success, fills *id and relay_cmd buffer.
// Uses sscanf with bounded width (%15s) to prevent overflow
// from a malformed or noise-corrupted line.

static uint8_t Parse_Command(const char *line, uint16_t *id, char *relay_cmd, size_t relay_cmd_size)
{
	const char *cmd_start = strstr(line, "CMD,");

	if(!cmd_start)
	{
		return 0;
	}

	unsigned int parsed_id;
	char parsed_relay[16];

	if(sscanf(cmd_start, "CMD,%u,%15[^,]", &parsed_id, parsed_relay) != 2)
	{
		return 0;
	}

	*id = (uint16_t)parsed_id;
	strncpy(relay_cmd, parsed_relay, relay_cmd_size - 1);

	relay_cmd[relay_cmd_size - 1] = '\0';

	return 1;
}

// NEW: sends compact ACK,<id>,<state>,<status> — NOT a JSON telemetry send.
// Single short attempt only — if this specific ACK is lost, the home node
// will auto-resend the command, we'll re-detect the duplicate, and re-send
// this ACK again at that point. No need to retry-loop here.

static uint8_t Send_Relay_Ack(uint16_t id, uint8_t state, const char *status)
{
	char ack_payload[32];
	snprintf(ack_payload, sizeof(ack_payload), "ACK,%u,%d,%s", id, state, status);

	char ack_cmd[64];
	int len = snprintf(ack_cmd, sizeof(ack_cmd), "AT+SEND=%d,%d,%s\r\n", LORA_DEST_ADDR, (int)strlen(ack_payload), ack_payload);

	if(len < 0 || (size_t)len >= sizeof(ack_cmd))
	{
		return 0;
	}

	Uart2_Debug("TX ACK: ");
	Uart2_Debug(ack_cmd);

	Lora_Send(ack_cmd);

	 // Single short wait for module's local +OK — not a full ResponseOK() loop

	uint32_t start = ms_ticks;

	while((ms_ticks - start) < 1000)
	{
		memset(resp, 0, sizeof(resp));
		if(Lora_ReadLine(resp, sizeof(resp), 300))
		{
			if(strstr(resp, "OK"))
			{
				return 1;
			}

			if(strstr(resp, "ERR"))
			{
				return 0;
			}
		}
	}

	return 0;
}


// REWRITTEN: now ID-based dedup instead of state-based dedup.
// Parses CMD,<id>,<action> — if id matches last_cmd_id, it's a
// duplicate: skip the relay pulse, just resend the same ACK so the
// home node can confirm and stop resending. If it's a new id, execute
// normally, record the id, then send ACK.

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

            if (strstr(resp, "+RCV=") == NULL)
            {
            	continue;
            }

            uint16_t id;
            char relay_cmd[16];

            if(!Parse_Command(resp, &id, relay_cmd, sizeof(relay_cmd)))
            {
            	DBG("Parse failed - not a CMD line\r\n");
            	continue;
            }

            if(id == last_cmd_id)
            {
            	DBG("DUPLICATE cmd id — resending ACK, no relay pulse\r\n");
            	Send_Relay_Ack(id, Latest_data.m, "DUP");
            	continue;
            }

            // ---- New command — execute ----

            if(strcmp(relay_cmd, "RELAY1") == 0)
            {
            	DBG("RELAY1 -> Motor ON\r\n");
            	 Relay_Pulse(START_RELAY);
            	 Latest_data.m = 1;
            }
            else if(strcmp(relay_cmd, "RELAY2") == 0)
            {
            	 DBG("RELAY2 -> Motor OFF\r\n");
            	 Relay_Pulse(STOP_RELAY);
            	 Latest_data.m = 0;
            }
            else
            {
            	DBG("Unknown relay command — ignoring\r\n");
            	continue;
            }

            // Record AFTER successful execution so a crash mid-pulse
            // doesn't mark it as done when it wasn't

            last_cmd_id = id;
            Send_Relay_Ack(id, Latest_data.m, "OK");

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
