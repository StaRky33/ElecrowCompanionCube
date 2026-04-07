# Wiring Notes

## I2C Bus

All I2C devices share a single bus on the Elecrow CrowPanel 1.28":

| Signal | GPIO |
|--------|------|
| SDA | IO4 |
| SCL | IO5 |

### Devices on the bus

| Device | I2C Address | Purpose |
|--------|-------------|---------|
| BM8563 | `0x51` | RTC (onboard) |
| PI4IOE5V6408 | `0x43` | I/O expander — controls backlight (onboard) |
| ENS160 | `0x53` | Air quality sensor (soldered externally) |
| AHT21 | `0x38` | Temperature & humidity (soldered externally) |

---

## ENS160 + AHT Soldering

The ENS160+AHT module has no dedicated external connector on this board.
Solder fine wires directly to the **BM8563 RTC pads** on the back of the PCB.

```
ENS160/AHT module          CrowPanel board (back)
─────────────────          ──────────────────────
VCC  ──────────────────►  3.3V pad
GND  ──────────────────►  GND pad
SDA  ──────────────────►  BM8563 SDA pad (IO4)
SCL  ──────────────────►  BM8563 SCL pad (IO5)
```

> Use the thinnest wire available (enamelled / magnet wire works well).
> Keep wires short to avoid I2C signal integrity issues.
> Verify continuity with a multimeter before powering up.

---

## Backlight

The display backlight is **not** controlled by a direct GPIO. It is routed through the **PI4IOE5V6408 I/O expander** via I2C. The firmware handles this automatically in `setup()` — no hardware change needed.

---

## Power

The board accepts a **3.7V LiPo battery** via its onboard SH1.0 2-pin connector.
Charging is handled by the onboard circuit via the **USB-C port**.
No external charging module is required.
