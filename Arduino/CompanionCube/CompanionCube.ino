#define LGFX_USE_V1
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LovyanGFX.hpp>
#include <I2C_BM8563.h>
#include "secrets.h"

/* ── WIFI ───────────────────────────────────────────────────── */
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* ntpServer = "pool.ntp.org";

/* ── I2C ────────────────────────────────────────────────────── */
#define I2C_SDA 4
#define I2C_SCL 5

/* ── IO EXPANDER ────────────────────────────────────────────── */
#define PI4IO_I2C_ADDR 0x43

/* ── BUZZER ─────────────────────────────────────────────────── */
#define BUZZER_PIN 3

/* ── RTC (BM8563) ───────────────────────────────────────────── */
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
I2C_BM8563_DateTypeDef rtcDate;
I2C_BM8563_TimeTypeDef rtcTime;

/* ── ENS160 + AHT - faked until soldered ───────────────────── */
#define ENS160_READY false

/* ── FAKE SENSOR DATA ───────────────────────────────────────── */
float fakeTemp = 21.5;
float fakeHum = 48.0;
uint16_t fakeEco2 = 650;
uint16_t fakeTvoc = 120;

/* ── LIVE DATA ──────────────────────────────────────────────── */
float temp = 21.5;
float hum = 48.0;
uint16_t eco2 = 650;
uint16_t tvoc = 120;

/* ── TIMERS ─────────────────────────────────────────────────── */
unsigned long lastSensor = 0;
unsigned long lastSerial = 0;

/* ── CLOCK CACHE ────────────────────────────────────────────── */
int prevHour = 0;
int prevMin = 0;
int prevSec = 0;

/* ── DISPLAY (LovyanGFX - GC9A01) ──────────────────────────── */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 6;
      cfg.pin_mosi = 7;
      cfg.pin_miso = -1;
      cfg.pin_dc = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 10;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

/* ── COLORS ─────────────────────────────────────────────────── */
#define BLACK 0x0000
#define WHITE 0xFFFF
#define CYAN 0x07FF
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define ORANGE 0xFD20
#define RED 0xF800
#define GRAY 0x8410
#define DKGRAY 0x4208

/* ══════════════════════════════════════════════════════════════
   IO EXPANDER - copied exactly from factory demo
   ══════════════════════════════════════════════════════════════ */
void init_IO_extender() {
  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(PI4IO_I2C_ADDR, 1);
  uint8_t rxdata = Wire.read();
  Serial.print("IO Expander ID: ");
  Serial.println(rxdata, HEX);

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x03);
  Wire.write((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
  Wire.endTransmission();

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x07);
  Wire.write(~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4)));
  Wire.endTransmission();
}

void set_pin_io(uint8_t pin_number, bool value) {
  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x05);
  Wire.endTransmission();
  Wire.requestFrom(PI4IO_I2C_ADDR, 1);
  uint8_t rxdata = Wire.read();

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x05);
  if (!value)
    Wire.write((~(1 << pin_number)) & rxdata);
  else
    Wire.write((1 << pin_number) | rxdata);
  Wire.endTransmission();
}

/* ══════════════════════════════════════════════════════════════
   BUZZER
   ══════════════════════════════════════════════════════════════ */
void beep(int freq, int time_ms) {
  tone(BUZZER_PIN, freq);
  delay(time_ms);
  noTone(BUZZER_PIN);
}

/* ══════════════════════════════════════════════════════════════
   WIFI + NTP
   ══════════════════════════════════════════════════════════════ */
void syncRTCFromNTP() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" connected!");
    configTime(2 * 3600, 0, ntpServer);

    struct tm timeinfo;
    int ntpAttempts = 0;
    while (!getLocalTime(&timeinfo) && ntpAttempts < 10) {
      delay(500);
      ntpAttempts++;
    }

    if (getLocalTime(&timeinfo)) {
      rtcTime.hours = timeinfo.tm_hour;
      rtcTime.minutes = timeinfo.tm_min;
      rtcTime.seconds = timeinfo.tm_sec;
      rtc.setTime(&rtcTime);

      rtcDate.year = timeinfo.tm_year + 1900;
      rtcDate.month = timeinfo.tm_mon + 1;
      rtcDate.date = timeinfo.tm_mday;
      rtc.setDate(&rtcDate);

      Serial.println("RTC synced from NTP.");
    } else {
      Serial.println("NTP sync failed.");
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected.");
  } else {
    Serial.println(" WiFi failed — RTC keeps existing time.");
  }
}

/* ══════════════════════════════════════════════════════════════
   CLOCK
   ══════════════════════════════════════════════════════════════ */
void updateClock() {
  rtc.getTime(&rtcTime);
  prevHour = rtcTime.hours;
  prevMin = rtcTime.minutes;
  prevSec = rtcTime.seconds;
}

/* ══════════════════════════════════════════════════════════════
   AIR SENSORS
   ══════════════════════════════════════════════════════════════ */
void readAir() {
  if (ENS160_READY) {
    // Uncomment once ENS160 + AHT are soldered:
    // sensors_event_t humidity, tempEvent;
    // if (aht.getEvent(&humidity, &tempEvent)) {
    //   temp = tempEvent.temperature;
    //   hum  = humidity.relative_humidity;
    // }
    // ens160.set_envdata(temp, hum);
    // ens160.measure();
    // eco2 = ens160.geteCO2();
    // tvoc = ens160.getTVOC();
  } else {
    fakeTemp += random(-5, 6) * 0.1f;
    fakeHum += random(-3, 4) * 0.1f;
    fakeTemp = constrain(fakeTemp, 18.0, 28.0);
    fakeHum = constrain(fakeHum, 35.0, 65.0);
    temp = fakeTemp;
    hum = fakeHum;
    eco2 = fakeEco2;
    tvoc = fakeTvoc;
  }
}

/* ══════════════════════════════════════════════════════════════
   AQI HELPERS
   ══════════════════════════════════════════════════════════════ */
int getAQILevel() {
  if (eco2 < 800) return 0;
  else if (eco2 < 1200) return 1;
  else if (eco2 < 1800) return 2;
  else return 3;
}

String getAQILabel(int level) {
  if (level == 0) return "Good";
  else if (level == 1) return "Moderate";
  else if (level == 2) return "Poor";
  else return "DANGER";
}

uint16_t getAQIColor(int level) {
  if (level == 0) return GREEN;
  else if (level == 1) return YELLOW;
  else if (level == 2) return ORANGE;
  else return RED;
}

/* ══════════════════════════════════════════════════════════════
   DISPLAY - static background
   ══════════════════════════════════════════════════════════════ */
void drawStaticUI()
{
  tft.fillScreen(WHITE);

  tft.drawCircle(120, 120, 118, DKGRAY);
  tft.drawCircle(120, 120, 116, DKGRAY);

  tft.drawFastHLine(25, 85,  190, DKGRAY);
  tft.drawFastHLine(25, 158, 190, DKGRAY);
  tft.drawFastVLine(120, 85, 73, DKGRAY);

  tft.setTextSize(1);
  tft.setTextColor(GRAY);

  tft.setCursor(34, 92);
  tft.print("HUMIDITY");

  tft.setCursor(130, 92);
  tft.print("TEMP");

  tft.setCursor(40, 164);
  tft.print("TVOC");

  tft.setCursor(130, 164);
  tft.print("eCO2");

  if (!ENS160_READY) {
    tft.setTextColor(DKGRAY);
    tft.setCursor(81, 220);
    tft.print("* SIMULATED *");
  }
}

void drawSplashScreen()
{
  tft.fillScreen(WHITE);

  int cx = 120;
  int cy = 125;

  tft.fillCircle(cx - 25, cy - 20, 30, 0xF800);  // RED
  tft.fillCircle(cx + 25, cy - 20, 30, 0xF800);

  tft.fillTriangle(
    cx - 52, cy - 5,
    cx + 52, cy - 5,
    cx,      cy + 50,
    0xF800
  );

  delay(2000);
}

/* ══════════════════════════════════════════════════════════════
   DISPLAY - live values
   ══════════════════════════════════════════════════════════════ */

void drawValues()
{
  int      level    = getAQILevel();
  uint16_t aqiColor = getAQIColor(level);

  // ── Time (centered) ──
  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", prevHour, prevMin, prevSec);
  tft.fillRect(40, 35, 160, 45, WHITE);
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(48, 48);
  tft.print(timeBuf);

  // ── Humidity (left cell) ──
  tft.fillRect(25, 105, 90, 48, WHITE);
  tft.setTextSize(2);
  tft.setTextColor(RED);
  tft.setCursor(42, 118);
  tft.print((int)hum);
  tft.print("%");

  // ── Temperature (right cell) ──
  tft.fillRect(122, 105, 90, 48, WHITE);
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(150, 118);
  tft.print(temp, 1);
  tft.print("C");

  // ── TVOC (left cell, more centered) ──
  tft.fillRect(30, 174, 84, 28, WHITE);  
  tft.setTextSize(2);
  tft.setTextColor(BLACK);
  // "120ppb" = 6 chars * 12px = 72px, center of left cell (25+90/2=70) = start at 34
  tft.setCursor(34, 178);
  tft.print(tvoc);
  tft.print("ppb");

  // ── eCO2 (right cell, more centered) ──
  tft.fillRect(126, 174, 84, 28, WHITE); 
  tft.setTextSize(2);
  tft.setTextColor(aqiColor);
  // "650ppm" = 6 chars * 12px = 72px, center of right cell (122+90/2=167) = start at 131
  tft.setCursor(131, 178);
  tft.print(eco2);
  tft.print("ppm");

  // ── AQI label (centered) ──
  tft.fillRect(60, 200, 120, 18, WHITE);
  tft.setTextSize(2);
  tft.setTextColor(aqiColor);
  String label = getAQILabel(level);
  int labelX = 120 - (label.length() * 6);
  tft.setCursor(labelX, 202);
  tft.print(label);

  if (level == 3) beep(3000, 40);
}


/* ══════════════════════════════════════════════════════════════
   SERIAL OUTPUT
   ══════════════════════════════════════════════════════════════ */
void printSerial() {
  int level = getAQILevel();
  Serial.println("================================");
  Serial.printf("Time     : %02d:%02d:%02d\n", prevHour, prevMin, prevSec);
  Serial.print("Temp     : ");
  Serial.print(temp, 1);
  Serial.println(ENS160_READY ? " C" : " C (fake)");
  Serial.print("Humidity : ");
  Serial.print(hum, 0);
  Serial.println(ENS160_READY ? " %" : " % (fake)");
  Serial.print("eCO2     : ");
  Serial.print(eco2);
  Serial.println(ENS160_READY ? " ppm" : " ppm (fake)");
  Serial.print("TVOC     : ");
  Serial.print(tvoc);
  Serial.println(ENS160_READY ? " ppb" : " ppb (fake)");
  Serial.print("Air      : ");
  Serial.print(getAQILabel(level));
  Serial.print(" (level ");
  Serial.print(level);
  Serial.println(")");
  Serial.println("RTC      : hardware BM8563");
  Serial.println("================================");
}

/* ══════════════════════════════════════════════════════════════
   SETUP
   ══════════════════════════════════════════════════════════════ */
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting Companion Cube...");

  // I2C init
  Wire.begin(I2C_SDA, I2C_SCL);

  // IO expander init
  init_IO_extender();
  delay(100);
  set_pin_io(3, true);
  set_pin_io(4, true);
  delay(100);

  // Display init
  tft.init();
  tft.setColorDepth(16);
  tft.endWrite();
  delay(200);

  // Backlight on after display init
  set_pin_io(2, true);
  delay(100);
  drawSplashScreen();
  // RTC init
  rtc.begin();
  Serial.println("BM8563 RTC ready.");

  // Sync RTC from NTP
  syncRTCFromNTP();

  // Initial sensor read
  readAir();

  // Draw UI
  drawStaticUI();

  // Boot beep
  beep(1000, 80);

  Serial.println("Setup complete.");
}
/* ══════════════════════════════════════════════════════════════
   LOOP
   ══════════════════════════════════════════════════════════════ */
void loop() {
  updateClock();

  if (millis() - lastSensor > 5000) {
    lastSensor = millis();
    readAir();
  }

  drawValues();

  if (millis() - lastSerial > 2000) {
    lastSerial = millis();
    printSerial();
  }

  delay(1000);
}
