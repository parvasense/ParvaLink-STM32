# 🌾 ParvaLink — Smart Farm IoT System

> Long-range wireless farm motor control and 3-phase power monitoring using STM32F401 (farm) + Raspberry Pi Pico W (home) + LoRa + MIT App Inventor

![Embedded C](https://img.shields.io/badge/Farm%20Node-STM32F401%20Bare--Metal%20C-blue?logo=stmicroelectronics)
![MicroPython](https://img.shields.io/badge/Home%20Node-MicroPython-yellow?logo=python)
![LoRa](https://img.shields.io/badge/LoRa-RYLR998-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

---

## 📖 Overview

**ParvaLink** is a low-cost, long-range IoT system designed for Indian farmers to remotely control irrigation pump motors and monitor 3-phase electrical parameters — without requiring internet at the farm location.

The system uses **LoRa radio** (865 MHz) to communicate between a home base station (Raspberry Pi Pico W, MicroPython) and a farm field unit (**STM32F401, bare-metal embedded C**). An Android app built with MIT App Inventor provides the user interface.

---

## ✨ Features

- 📡 **Long-range LoRa communication** — no internet needed at farm
- ⚡ **3-phase power monitoring** — R, Y, B phase Amps and Volts in real time
- 🌡️ **Temperature and humidity monitoring** — DHT11 sensor at farm
- 🔁 **Remote motor START/STOP** — 1-second relay pulse from phone
- ✅ **Motor status confirmation** — app shows ON/OFF within 1–2 seconds
- 🔄 **Auto-retry on packet loss** — up to 3 automatic retries, no manual pressing
- 🛡️ **Command ID–based idempotency** — duplicate commands rejected via last_cmd_id tracking on the farm node
- ⚙️ **ISR-driven UART ring buffers** — non-blocking, interrupt-based LoRa RX/TX on the farm node
- 📱 **Android app** — built with MIT App Inventor, no coding required to use
- 💾 **IP address memory** — saved via TinyDB, auto-loads on app start

---

## 🏗️ System Architecture

```
┌─────────────────────┐         LoRa 865 MHz          ┌─────────────────────┐
│     HOME NODE       │ ◄────────────────────────────► │     FARM NODE       │
│   Raspberry Pi      │                                 │     STM32F401       │
│     Pico W          │                                 │  (Bare-Metal C)     │
│                     │                                 │                     │
│  • Wi-Fi HTTP server│                                 │  • DHT11 sensor     │
│  • LoRa receiver    │                                 │  • 3-phase monitor  │
│  • Relay commander  │                                 │  • Relay1 (START)   │
│  • Auto-retry logic │                                 │  • Relay2 (STOP)    │
│  (MicroPython)      │                                 │  • ISR UART ring    │
│                     │                                 │    buffer + CMD/ACK │
└────────┬────────────┘                                 └─────────────────────┘
         │ Wi-Fi (HTTP)
         │ 192.168.1.x
┌────────▼────────────┐
│   ANDROID APP       │
│  MIT App Inventor   │
│                     │
│  • Live sensor data │
│  • START / STOP     │
│  • Motor status     │
│  • IP address setup │
└─────────────────────┘
```

---

## 🔧 Hardware Required

| Component | Quantity | Purpose |
|-----------|----------|---------|
| Raspberry Pi Pico W | 1 | Home node — Wi-Fi + LoRa bridge (MicroPython) |
| STM32F401 (e.g. Black Pill) | 1 | Farm node — sensors + relay control (bare-metal C) |
| RYLR998 LoRa Module | 2 | Long-range radio communication |
| DHT11 Sensor | 1 | Temperature and humidity at farm |
| 2-Channel Relay Module | 1 | Motor START and STOP contactors |
| Android Phone | 1 | MIT App Inventor app |

---

## 📌 Pin Connections

### Farm Node (STM32F401)

| Pin | Connection |
|-----|-----------|
| PA9 (USART1 TX) | RYLR998 RX |
| PA10 (USART1 RX) | RYLR998 TX |
| PA0 | DHT11 Data |
| PB0 | Relay1 — Motor START |
| PB1 | Relay2 — Motor STOP |
| 3.3V | RYLR998 VCC, DHT11 VCC |
| GND | Common Ground |

> Note: confirm exact GPIO/USART mapping against your board's schematic before wiring — adjust the pin table above if your STM32F401 layout differs.

### Home Node (Pico W)

| Pin | Connection |
|-----|-----------|
| GP0 (TX) | RYLR998 RX |
| GP1 (RX) | RYLR998 TX |
| 3.3V | RYLR998 VCC |
| GND | Common Ground |

---

## 📡 LoRa Configuration

| Parameter | Value |
|-----------|-------|
| Band | 865 MHz (India) |
| Network ID | 5 |
| Home Node Address | 1 |
| Farm Node Address | 2 |
| Spreading Factor | SF9 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| TX Power | 22 dBm |

---

## 📦 JSON Data Packet

The farm node sends this JSON every 5 seconds:

```json
{
  "t": 1609459521,
  "T": 34,
  "H": 53,
  "m": 1,
  "p": [
    {"a": 10.5, "v": 435.6},
    {"a": 10.2, "v": 437.5},
    {"a": 10.8, "v": 438.7}
  ]
}
```

| Key | Meaning |
|-----|---------|
| `t` | Unix timestamp |
| `T` | Temperature (°C) |
| `H` | Humidity (%) |
| `m` | Motor status — 1=ON, 0=OFF |
| `p` | Phase array — R, Y, B phases |
| `p[n].a` | Phase current (Amps) |
| `p[n].v` | Phase voltage (Volts) |

---

## 🌐 HTTP API Endpoints

The Home Node runs a simple HTTP server on port 80:

| Endpoint | Method | Response | Purpose |
|----------|--------|----------|---------|
| `/data` | GET | Full JSON | All sensor data |
| `/status` | GET | `{"m":0}` or `{"m":1}` | Motor state only |
| `/relay?id=1&state=0` | GET | `OK` or `RETRY` | Start motor |
| `/relay?id=2&state=0` | GET | `OK` or `RETRY` | Stop motor |
| `/ping` | GET | `pong` | Connectivity check |

---

## 🔄 Relay Reliability Logic

The system uses a **confirm-and-retry** approach on the home node, backed by a **command ID–based idempotency check** on the STM32F401 farm node, to handle LoRa packet loss:

```
App presses START
        ↓
Home sends RELAY1 (attempt 1, cmd_id=N)
        ↓
Wait up to 4s for farm ACK (m=1 in JSON)
        ↓
    ┌───┴───┐
  ACK     No ACK
received  in 4s
    │         │
    ↓         ↓
CONFIRMED  Wait 1s → retry (attempt 2, same cmd_id)
  m=1              │
                   ↓
             Wait 4s for ACK
                   │
              ┌────┴────┐
            ACK       No ACK
              │           │
              ↓           ↓
         CONFIRMED    Retry (attempt 3, same cmd_id)
                           │
                      Final attempt
                           │
                    ┌──────┴──────┐
                  ACK           No ACK
                    │               │
                    ↓               ↓
               CONFIRMED      Return RETRY
                               to app
```

The STM32F401 farm node tracks `last_cmd_id` — even if the home node resends the same RELAY1 command across retries, the farm node fires the relay only once per unique command ID.

---

## 📱 App Features

- Live display of R, Y, B phase Amps and Volts
- Temperature and humidity display
- Motor status label — ON ✓ (green) / OFF ✗ (red) / ⚠ No Signal (yellow)
- START and STOP relay buttons
- IP address setup with TinyDB memory
- Automatic data polling every 2 seconds via Clock timer
- Separate Web components for data polling (Web1) and relay commands (Web2)

---

## 📁 Project Structure

```
ParvaLink/
├── farm_node/                # STM32F401 — bare-metal C project
│   ├── main.c                 # Main loop, sensor reads, relay control
│   ├── uart_ring_buffer.c/.h  # ISR-driven UART RX/TX ring buffer
│   ├── lora.c/.h              # RYLR998 AT command interface
│   └── cmd_protocol.c/.h      # CMD/ACK protocol, last_cmd_id dedup logic
├── home_node_final.py         # Home node — Wi-Fi HTTP server, relay confirm+retry (MicroPython)
└── README.md                  # This file
```

---

## 🚀 Setup Instructions

### 1. Set Up the Farm Node (STM32F401)

- Build and flash the `farm_node/` bare-metal C project using your preferred toolchain (e.g. STM32CubeIDE, or `arm-none-eabi-gcc` + `st-flash`/OpenOCD via an ST-Link programmer)
- Verify FPU is enabled (`SCB->CPACR`) if using floating-point sensor math

### 2. Set Up the Home Node (Pico W)

Flash MicroPython firmware from [micropython.org](https://micropython.org/download/), then update Wi-Fi credentials in `home_node_final.py`:

```python
WIFI_SSID     = "your_wifi_name"
WIFI_PASSWORD = "your_wifi_password"
```

Flash `home_node_final.py` as `main.py` on the Pico W.

### 3. Install the App

- Download the `.apk` from the releases section
- Or open the `.aia` file in [MIT App Inventor](https://ai2.appinventor.mit.edu) and build it yourself

### 4. Set IP Address

- Power on the Pico W and STM32F401
- Check serial monitor for the Pico W IP address (e.g. `192.168.1.8`)
- Open the app → tap **Set IP Address** → enter the IP → tap **Confirm IP**

---

## ⚙️ Tuning Parameters

In `home_node_final.py` (Pico W, MicroPython):

```python
RELAY_CONFIRM_TIMEOUT_MS = 4000   # Wait time per attempt (ms)
RELAY_MAX_RETRIES        = 2      # Number of retries after first attempt
RELAY_RETRY_DELAY_MS     = 1000   # Delay between retries (ms)
```

In `farm_node/main.c` (STM32F401, bare-metal C):

```c
#define SEND_INTERVAL_MS   5000   // JSON send interval (ms)
#define RELAY_DEDUP_MS     8000   // Duplicate command rejection window (ms)
```

---

## 🐛 Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Relay not firing | LoRa packet loss | Auto-retry handles it — wait 10s |
| Motor status wrong | Delayed ACK | Wait for next 5s JSON packet |
| Error 1101 in app | Server busy during relay confirm | Normal — data updates after relay completes |
| No Signal in app | Pico W not reachable | Check Wi-Fi, check IP address in app |
| LoRa not initialising | Wiring issue | Check TX/RX pins, check 3.3V supply |
| DHT11 error | Sensor fault | Check farm node GPIO connection, replace sensor |
| Garbled/no UART data on farm node | Ring buffer or baud mismatch | Check ISR priority, verify USART baud rate config |

---

## 📊 Performance

| Metric | Value |
|--------|-------|
| Data update interval | 5 seconds |
| Relay response time (normal) | 1–2 seconds |
| Relay response time (retry) | 5–14 seconds |
| Motor status confirmation | Within 1–2 seconds of relay fire |
| Packet loss handling | Up to 3 automatic retries |

---

## 🔒 Relay Safety

- Active-LOW relay board — relay is OFF when pin is HIGH (safe default on boot)
- 1-second pulse only — relay does not stay latched by firmware
- Command ID–based deduplication on the STM32F401 farm node prevents accidental double fire
- Separate START and STOP relays — matches motor contactor wiring

---

## 📜 License

MIT License — free to use, modify, and distribute.

---

## 👤 Author

**Parvasense** — [parvasenseofficial@gmail.com](mailto:parvasenseofficial@gmail.com)

---

## 🙏 Acknowledgements

- [STM32F401 / STM32CubeIDE](https://www.st.com/) — STMicroelectronics
- [MicroPython](https://micropython.org/) — Python for microcontrollers
- [RYLR998 LoRa Module](https://reyax.com/) — Reyax Technology
- [MIT App Inventor](https://appinventor.mit.edu/) — MIT CSAIL
- [Raspberry Pi Foundation](https://www.raspberrypi.org/) — Pico W

---

*Built for Indian farmers — affordable, reliable, no internet required at the farm.*
