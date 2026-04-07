# Elecrow Companion Cube

A DIY smart desk clock built on the **Elecrow CrowPanel ESP32 1.28" Round Display**, inspired by [Huy Vector's Smart Cyber Clock V2](https://www.huyvector.org/smart-cyber-clock-v2) and shaped like a Portal Companion Cube.

Displays real-time clock, temperature, humidity, air quality (eCO2/TVOC), with an onboard buzzer alert for dangerous CO2 levels. Time is maintained by a hardware RTC (BM8563) with battery backup — no network required after initial sync.

---

## Features

- Real-time clock via **BM8563 RTC** (coin cell backup, survives power loss)
- NTP sync on first boot to set the RTC (France / GMT+2 CEST)
- Air quality monitoring: **temperature, humidity, eCO2, TVOC** via ENS160 + AHT sensor
- CO2 danger alert via onboard **buzzer**
- Round 240×240 IPS display with clean sensor dashboard
- Fully offline after initial WiFi sync

---

## Hardware

| Component | Details |
|-----------|---------|
| Main board | Elecrow CrowPanel ESP32-C3 1.28" Round Display |
| Air quality sensor | ENS160 + AHT21 combo module |
| Display | GC9A01 round IPS 240×240 (onboard) |
| RTC | BM8563 with CR927 coin cell (onboard) |
| Buzzer | Passive buzzer (onboard, IO3) |

> See [`BOM.md`](BOM.md) for full bill of materials with links.

---

## Wiring

The ENS160+AHT sensor connects directly to the BM8563 RTC pads on the back of the board:

| Sensor pin | Board pad |
|------------|-----------|
| SDA | BM8563 SDA (IO4) |
| SCL | BM8563 SCL (IO5) |
| VCC | 3.3V |
| GND | GND |

> All three devices (BM8563, ENS160, AHT) share the same I2C bus.  
> See [`wiring.md`](wiring.md) for detailed soldering instructions and I2C addresses.

---

## Software

### Requirements

- Arduino IDE 2.x
- Board: **ESP32C3 Dev Module** (esp32 by Espressif Systems 2.0.14+)
- **USB CDC On Boot**: Enabled (required for Serial Monitor)

### Libraries

Install via Arduino Library Manager:

| Library | Purpose |
|---------|---------|
| `Arduino_GFX_Library` | GC9A01 display driver |
| `I2C BM8563` | RTC chip |
| `Adafruit AHTX0` | Temperature & humidity |
| `ScioSense ENS160` | eCO2 & TVOC |
| `Adafruit BusIO` | I2C/SPI dependency |

### Configuration

In `Arduino/CompanionCube.ino`, set your WiFi credentials before uploading:

```cpp
const char* ssid     = "your_wifi_name";
const char* password = "your_wifi_password";
```

The timezone is set to **France (GMT+2 CEST)**:
```cpp
configTime(2 * 3600, 0, ntpServer);
```
Adjust the multiplier if needed (e.g. `1 * 3600` for winter CET).

Once the RTC is synced from NTP on first boot, WiFi is disconnected and the clock runs fully offline.

### ENS160 status

The ENS160+AHT sensor is enabled via a `#define` flag at the top of the sketch:

```cpp
#define ENS160_READY false  // set to true once sensor is soldered
```

When `false`, the display shows simulated sensor values labeled `(fake)`.

---

## Project Structure

```
Elecrow-Companion-Cube/
|   .gitignore
|   BOM.md
|   LICENSE
|   README.md
|   wiring.md
|
+---3DModels
|       Angle.stl
|       Body_plate.stl
|       CrowPanel ESP32 Display1.28_asm.stl
|       CrowPanel ESP32 Display1.28_asm.stp
|       Edge.stl
|       Heart_insert.stl
|       Heart_medal.stl
|       Weighted_Companion_Cube.stl
|
+---Arduino
|       CompanionCube.ino
|
\---Docs
        CrowPanel ESP32 Display 1.28(R) Inch_V1.0_240507.brd
        CrowPanel ESP32 Display 1.28(R) Inch_V1.0_240507.pdf
        PCB_BM8563.png
        PCB_BM8563_pinout.png
```

---

## License

MIT — see [`LICENSE`](LICENSE)

---

## Credits

Original design concept by [Huy Vector](https://www.huyvector.org/smart-cyber-clock-v2).
Original Companion Cube by [Poh](https://www.thingiverse.com/thing:33138)
Remixed Companion Cube by [Nuk1591](https://www.thingiverse.com/thing:2981843)
