# Bill of Materials

All components used in the Smart Cyber Clock V2 build.

---

## Main Board

| Component | Description | Link |
|-----------|-------------|------|
| Elecrow CrowPanel ESP32 1.28" | ESP32-C3, GC9A01 round display 240×240, BM8563 RTC, buzzer, CR927 battery holder, Type-C, LiPo connector | [Elecrow](https://www.elecrow.com/crowpanel-esp32-display-1-28-r-inch-240-240-round-ips-display-capacitive-touch-spi-screen.html) |
| CR927 coin cell | Powers the BM8563 RTC when the board is off | Local / Amazon |

---

## Sensors

| Component | Description | Notes |
|-----------|-------------|-------|
| ENS160 + AHT21 combo module | eCO2, TVOC, temperature, humidity over I2C | Soldered directly to BM8563 SDA/SCL pads |

---

## Power

| Component | Description | Notes |
|-----------|-------------|-------|
| LiPo battery | 3.7V, any capacity (recommended 500mAh+) | Plugs into onboard SH1.0 2-pin connector |
| USB-C cable | For charging and firmware upload | Standard |

> The Elecrow board has an integrated LiPo charger — no external charging module needed.

---

## Tools

| Tool | Purpose |
|------|---------|
| Soldering iron | Attaching ENS160 wires to BM8563 pads |
| Fine enamelled wire / magnet wire | I2C connections to SMD pads |
| Multimeter | Continuity check before powering up |

---

## Notes

- The ENS160+AHT module shares the I2C bus with the onboard BM8563 RTC (`SDA=IO4`, `SCL=IO5`)
- Make sure your ENS160 breakout board has onboard pull-up resistors (most do). If not, add 4.7kΩ from SDA and SCL to 3.3V.
- The backlight on this board is controlled via the **PI4IOE5V6408 I/O expander** (I2C address `0x43`), not a direct GPIO pin.
