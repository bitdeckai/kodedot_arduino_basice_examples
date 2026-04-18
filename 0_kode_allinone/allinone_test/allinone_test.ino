/*
 * KODE all-in-one functional test sketch.
 * Integrates examples from folders 1..10 so one firmware image can test all modules.
 */

#include <Arduino.h>
#include <Wire.h>
#include <FS.h>
#include <SD_MMC.h>
#include <ESP_I2S.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <bb_captouch.h>
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_LIS2MDL.h>
#include <kode_MAX31329.h>
#include <PMIC_BQ25896.h>
#include <BQ27220.h>
#include <esp_heap_caps.h>
#include <stdarg.h>

// Shared pin map
static const int PIN_I2C_SDA = 48;
static const int PIN_I2C_SCL = 47;
static const int PIN_EXP_ADDR = 0x20;
static const int PIN_EXP_INT = 18;

static const int PIN_SD_CMD = 5;
static const int PIN_SD_CLK = 6;
static const int PIN_SD_D0 = 7;

static const int PIN_I2S_SCK = 38;
static const int PIN_I2S_WS = 45;
static const int PIN_I2S_DIN = 21;
static const int PIN_I2S_DOUT = 46;

static const int PIN_NEOPIXEL = 4;
static const int PIN_BUTTON_UP = 0;

// Display pin map
static const int DSP_HOR_RES = 410;
static const int DSP_VER_RES = 502;
static const int DSP_SCLK = 17;
static const int DSP_SDIO0 = 15;
static const int DSP_SDIO1 = 14;
static const int DSP_SDIO2 = 16;
static const int DSP_SDIO3 = 10;
static const int DSP_RST = 8;
static const int DSP_CS = 9;
static const int DSP_BL = 22;

static const uint32_t TEST_WINDOW_MS = 3000;

// TCA95xx 16-bit register map (TCA9555 compatible)
static const uint8_t TCA_REG_INPUT0 = 0x00;
static const uint8_t TCA_REG_OUTPUT0 = 0x02;
static const uint8_t TCA_REG_POLARITY0 = 0x04;
static const uint8_t TCA_REG_CONFIG0 = 0x06;

// Global devices
Adafruit_NeoPixel gPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
I2SClass gI2S;
Adafruit_LSM6DSOX gImu;
Adafruit_LIS2MDL gMag(12345);
MAX31329 gRtc;
PMIC_BQ25896 gPmic;
BQ27220 gGauge;

static Arduino_DataBus *gGfxBus = nullptr;
static Arduino_CO5300 *gGfx = nullptr;
static BBCapTouch gTouch;

static lv_display_t *gLvDisplay = nullptr;
static lv_indev_t *gLvIndev = nullptr;
static uint8_t *gLvBuf1 = nullptr;
static uint8_t *gLvBuf2 = nullptr;
static bool gLvglReady = false;
static bool gSdMounted = false;
static bool gExpanderReady = false;
static uint16_t gExpanderOutput = 0x0000;
static uint16_t gExpanderConfig = 0xFFFF;
static int gScreenLogLine = 0;
static const int SCREEN_LOG_LINE_H = 18;
static const int SCREEN_LOG_MARGIN = 8;

static void screenLog(const char *msg) {
  if (gGfx == nullptr || msg == nullptr) {
    return;
  }

  int maxLines = (DSP_VER_RES - SCREEN_LOG_MARGIN * 2) / SCREEN_LOG_LINE_H;
  if (maxLines < 1) {
    maxLines = 1;
  }

  if (gScreenLogLine >= maxLines) {
    gGfx->fillScreen(0x0000);
    gScreenLogLine = 0;
  }

  int y = SCREEN_LOG_MARGIN + gScreenLogLine * SCREEN_LOG_LINE_H;
  gGfx->setTextColor(0xFFFF, 0x0000);
  gGfx->setTextSize(2);
  gGfx->setCursor(SCREEN_LOG_MARGIN, y);
  gGfx->println(msg);
  gScreenLogLine++;
}

static void logBoth(const char *msg) {
  if (msg == nullptr) {
    return;
  }
  Serial.println(msg);
  screenLog(msg);
}

static void logBothf(const char *fmt, ...) {
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  logBoth(buf);
}

volatile bool gExpanderInterrupted = false;
volatile bool gButtonUpInterrupted = false;

static void printBanner(const char *title) {
  Serial.println();
  Serial.println("=================================================");
  Serial.println(title);
  Serial.println("=================================================");
}

void IRAM_ATTR handleExpanderIRQ() {
  gExpanderInterrupted = true;
}

void IRAM_ATTR handleButtonUpIRQ() {
  gButtonUpInterrupted = true;
}

static uint32_t lvTickMs() {
  return millis();
}

static void lvglFlushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  (void)disp;
  if (gGfx == nullptr) {
    return;
  }

  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  gGfx->startWrite();
  gGfx->writeAddrWindow(area->x1, area->y1, w, h);
  gGfx->writePixels((uint16_t *)px_map, w * h);
  gGfx->endWrite();

  lv_display_flush_ready(disp);
}

static void lvglTouchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  TOUCHINFO ti;
  if (gTouch.getSamples(&ti) && ti.count > 0) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = ti.x[0];
    data->point.y = ti.y[0];
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void ensureI2C() {
  static bool initialized = false;
  if (!initialized) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    initialized = true;
    Serial.println("I2C initialized on GPIO48/GPIO47");
  }
}

static bool expanderWrite8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PIN_EXP_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool expanderRead8(uint8_t reg, uint8_t *value) {
  Wire.beginTransmission(PIN_EXP_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(PIN_EXP_ADDR, 1) != 1) {
    return false;
  }

  *value = Wire.read();
  return true;
}

static bool expanderWrite16(uint8_t regLow, uint16_t value) {
  return expanderWrite8(regLow, (uint8_t)(value & 0xFF)) &&
         expanderWrite8((uint8_t)(regLow + 1), (uint8_t)((value >> 8) & 0xFF));
}

static bool expanderRead16(uint8_t regLow, uint16_t *value) {
  uint8_t lo = 0;
  uint8_t hi = 0;
  if (!expanderRead8(regLow, &lo) || !expanderRead8((uint8_t)(regLow + 1), &hi)) {
    return false;
  }
  *value = (uint16_t)lo | ((uint16_t)hi << 8);
  return true;
}

static bool expanderPinMode(int pin, uint8_t mode) {
  if (pin < 0 || pin > 15) {
    return false;
  }

  uint16_t mask = (uint16_t)1U << pin;
  if (mode == OUTPUT) {
    gExpanderConfig &= (uint16_t)~mask;
  } else {
    gExpanderConfig |= mask;
  }

  return expanderWrite16(TCA_REG_CONFIG0, gExpanderConfig);
}

static bool expanderDigitalWrite(int pin, int level) {
  if (pin < 0 || pin > 15) {
    return false;
  }

  uint16_t mask = (uint16_t)1U << pin;
  if (level == HIGH) {
    gExpanderOutput |= mask;
  } else {
    gExpanderOutput &= (uint16_t)~mask;
  }

  return expanderWrite16(TCA_REG_OUTPUT0, gExpanderOutput);
}

static int expanderDigitalRead(int pin) {
  if (pin < 0 || pin > 15) {
    return HIGH;
  }

  uint16_t inputs = 0;
  if (!expanderRead16(TCA_REG_INPUT0, &inputs)) {
    return HIGH;
  }

  return (inputs & ((uint16_t)1U << pin)) ? HIGH : LOW;
}

static bool setupExpander() {
  if (gExpanderReady) {
    return true;
  }

  ensureI2C();
  Wire.beginTransmission(PIN_EXP_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("Expander probe failed");
    return false;
  }

  // Disable inversion, default all pins as inputs and outputs low.
  if (!expanderWrite16(TCA_REG_POLARITY0, 0x0000) ||
      !expanderWrite16(TCA_REG_OUTPUT0, gExpanderOutput) ||
      !expanderWrite16(TCA_REG_CONFIG0, gExpanderConfig)) {
    Serial.println("Expander register init failed");
    return false;
  }

  // Keep peripherals and amplifier powered for later tests.
  if (!expanderPinMode(3, OUTPUT) || !expanderPinMode(4, OUTPUT) ||
      !expanderDigitalWrite(3, HIGH) || !expanderDigitalWrite(4, HIGH)) {
    Serial.println("Expander power pin setup failed");
    return false;
  }

  gExpanderReady = true;

  Serial.println("Expander initialized (0x20)");
  return true;
}

static bool mountSdIfNeeded() {
  if (gSdMounted) {
    return true;
  }

  if (!SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0)) {
    Serial.println("SD_MMC pin mapping failed");
    return false;
  }

  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD_MMC mount failed");
    return false;
  }

  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("SD_MMC card not detected");
    return false;
  }

  gSdMounted = true;
  Serial.printf("SD_MMC mounted: size=%lluMB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
  return true;
}

static void test1Esp32Info() {
  printBanner("[1] ESP32-S3 Info");
  Serial.printf("ESP32 model: %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU cores: %d\n", ESP.getChipCores());
  Serial.printf("Chip ID: %llu\n", ESP.getEfuseMac());
}

static void test2IoExpander() {
  printBanner("[2] IO Expander");
  if (!setupExpander()) {
    Serial.println("SKIP: expander not ready");
    return;
  }

  const int pins[14] = {0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  const char *names[14] = {
    "Magnetometer INT", "RTC INTB", "RTC INTA", "Battery GPOUT",
    "Pad Up", "Pad Left", "Pad Down", "Button Bottom",
    "PMIC INT", "Pad Right", "IMU INT2", "IMU INT1",
    "microSD CD", "Display TP INT"
  };

  for (int i = 0; i < 14; ++i) {
    expanderPinMode(pins[i], INPUT);
  }

  for (int i = 0; i < 14; ++i) {
    int level = expanderDigitalRead(pins[i]);
    Serial.printf("EXP%02d=%d  %s\n", pins[i], level, names[i]);
  }
}

static bool setupDisplayIfNeeded() {
  if (gGfx != nullptr) {
    return true;
  }

  gGfxBus = new Arduino_ESP32QSPI(DSP_CS, DSP_SCLK, DSP_SDIO0, DSP_SDIO1, DSP_SDIO2, DSP_SDIO3);
  gGfx = new Arduino_CO5300(gGfxBus, DSP_RST, 0, DSP_HOR_RES, DSP_VER_RES, DSP_BL, 0, 0, 0);

  if (gGfx == nullptr || !gGfx->begin()) {
    Serial.println("Display init failed");
    return false;
  }

  gGfx->setRotation(0);
  gGfx->setBrightness(255);
  gGfx->displayOn();

  pinMode(DSP_BL, OUTPUT);
  digitalWrite(DSP_BL, HIGH);

  Serial.println("Display initialized");
  return true;
}

static void test3DisplayAndLvgl() {
  printBanner("[3] Display + LVGL");

  if (!setupDisplayIfNeeded()) {
    Serial.println("SKIP: display unavailable");
    return;
  }

  // Display test from display_test.ino
  gGfx->fillScreen(0x001F);
  gGfx->setTextColor(0xFD20);
  gGfx->setTextSize(4);
  gGfx->setCursor(65, 250);
  gGfx->print("Hello World!");
  delay(500);

  // Minimal LVGL test from lvgl_test.ino
  if (!gLvglReady) {
    lv_init();
    lv_tick_set_cb(lvTickMs);

    ensureI2C();
    if (gTouch.init(PIN_I2C_SDA, PIN_I2C_SCL, -1, -1, 400000) == CT_SUCCESS) {
      gTouch.setOrientation(0, DSP_HOR_RES, DSP_VER_RES);
      Serial.println("Touch initialized");
    } else {
      Serial.println("Touch init failed, continue without touch");
    }

    gLvDisplay = lv_display_create(DSP_HOR_RES, DSP_VER_RES);
    lv_display_set_flush_cb(gLvDisplay, lvglFlushCb);

    size_t drawBuf = (DSP_HOR_RES * DSP_VER_RES / 10) * (LV_COLOR_DEPTH / 8);
    gLvBuf1 = (uint8_t *)heap_caps_malloc(drawBuf, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    gLvBuf2 = (uint8_t *)heap_caps_malloc(drawBuf, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (gLvBuf1 == nullptr || gLvBuf2 == nullptr) {
      Serial.println("LVGL buffer allocation failed");
      return;
    }

    lv_display_set_buffers(gLvDisplay, gLvBuf1, gLvBuf2, drawBuf, LV_DISPLAY_RENDER_MODE_PARTIAL);

    gLvIndev = lv_indev_create();
    lv_indev_set_type(gLvIndev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(gLvIndev, lvglTouchReadCb);

    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello Arduino, I am LVGL!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    gLvglReady = true;
    Serial.println("LVGL initialized");
  }

  uint32_t start = millis();
  while (millis() - start < 1000) {
    lv_timer_handler();
    delay(5);
  }
}

static void test4RgbLed() {
  printBanner("[4] Addressable LED");

  gPixel.begin();
  gPixel.clear();
  gPixel.show();

  uint32_t colors[3] = {
    gPixel.Color(150, 0, 0),
    gPixel.Color(0, 150, 0),
    gPixel.Color(0, 0, 150)
  };

  for (int i = 0; i < 3; ++i) {
    gPixel.setPixelColor(0, colors[i]);
    gPixel.show();
    delay(300);
    gPixel.setPixelColor(0, 0);
    gPixel.show();
    delay(200);
  }
}

static void test5Buttons() {
  printBanner("[5] Buttons");
  if (!setupExpander()) {
    Serial.println("SKIP: expander not ready");
    return;
  }

  expanderPinMode(6, INPUT);
  expanderPinMode(7, INPUT);
  expanderPinMode(8, INPUT);
  expanderPinMode(9, INPUT);
  expanderPinMode(11, INPUT);

  pinMode(PIN_EXP_INT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_EXP_INT), handleExpanderIRQ, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_UP), handleButtonUpIRQ, FALLING);

  Serial.println("Press buttons now (3 seconds)...");
  uint32_t start = millis();
  while (millis() - start < TEST_WINDOW_MS) {
    if (gButtonUpInterrupted) {
      gButtonUpInterrupted = false;
      Serial.println("BUTTON_UP pressed");
    }

    if (gExpanderInterrupted) {
      gExpanderInterrupted = false;
      if (expanderDigitalRead(6) == LOW) Serial.println("PAD_UP pressed");
      if (expanderDigitalRead(7) == LOW) Serial.println("PAD_LEFT pressed");
      if (expanderDigitalRead(8) == LOW) Serial.println("PAD_DOWN pressed");
      if (expanderDigitalRead(9) == LOW) Serial.println("BUTTON_BOTTOM pressed");
      if (expanderDigitalRead(11) == LOW) Serial.println("PAD_RIGHT pressed");
    }

    if (gLvglReady) {
      lv_timer_handler();
    }
    delay(5);
  }

  detachInterrupt(digitalPinToInterrupt(PIN_EXP_INT));
  detachInterrupt(digitalPinToInterrupt(PIN_BUTTON_UP));
}

static void test6MicroSd() {
  printBanner("[6] microSD");
  if (!mountSdIfNeeded()) {
    Serial.println("SKIP: SD card not available");
    return;
  }

  File f = SD_MMC.open("/allinone.txt", FILE_WRITE);
  if (!f) {
    Serial.println("Open /allinone.txt failed");
    return;
  }
  f.print("all-in-one test\n");
  f.close();

  f = SD_MMC.open("/allinone.txt");
  if (!f) {
    Serial.println("Read /allinone.txt failed");
    return;
  }

  Serial.print("/allinone.txt: ");
  while (f.available()) {
    Serial.write(f.read());
  }
  Serial.println();
  f.close();

  Serial.printf("SD total=%lluMB used=%lluMB\n",
                SD_MMC.totalBytes() / (1024ULL * 1024ULL),
                SD_MMC.usedBytes() / (1024ULL * 1024ULL));
}

static bool beginI2STx() {
  gI2S.setPins(PIN_I2S_SCK, PIN_I2S_WS, PIN_I2S_DOUT, -1);
  return gI2S.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_32BIT,
                    I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
}

static bool beginI2SRx() {
  gI2S.setPins(PIN_I2S_SCK, PIN_I2S_WS, -1, PIN_I2S_DIN);
  return gI2S.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_32BIT,
                    I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
}

static bool beginI2SMonitor() {
  gI2S.setPins(PIN_I2S_SCK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN);
  return gI2S.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_32BIT,
                    I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
}

static void test7Audio() {
  printBanner("[7] Audio (speaker + microphone)");

  if (!setupExpander()) {
    Serial.println("Expander missing, speaker amplifier may stay disabled");
  } else {
    expanderPinMode(3, OUTPUT);
    expanderDigitalWrite(3, HIGH);
  }

  if (!beginI2STx()) {
    Serial.println("SKIP: I2S TX init failed");
  } else {
    const int frequency = 300;
    int32_t sample = 500;
    int count = 0;

    Serial.println("Playing 2s square wave...");
    uint32_t start = millis();
    while (millis() - start < 2000) {
      if (count % (48000 / (2 * frequency)) == 0) {
        sample = -sample;
      }
      gI2S.write(sample);
      ++count;
    }

    gI2S.end();
  }

  if (!beginI2SMonitor()) {
    Serial.println("SKIP: I2S monitor init failed");
  } else {
    Serial.println("Live monitor: microphone to speaker (3s)...");
    gI2S.setTimeout(10);

    uint32_t start = millis();
    uint32_t frames = 0;
    while (millis() - start < 3000) {
      int32_t sample = 0;
      size_t n = gI2S.readBytes((char *)&sample, sizeof(sample));
      if (n == sizeof(sample)) {
        // Slight attenuation to reduce clipping/feedback.
        sample /= 2;
        gI2S.write(sample);
        ++frames;
      }

      if (gLvglReady) {
        lv_timer_handler();
      }
    }

    gI2S.end();
    Serial.printf("Live monitor finished, frames=%u\n", (unsigned int)frames);
  }

  if (!mountSdIfNeeded()) {
    Serial.println("SKIP: audio record (no SD card)");
    return;
  }

  if (!beginI2SRx()) {
    Serial.println("SKIP: I2S RX init failed");
    return;
  }

  Serial.println("Recording 3s to /allinone_mic.wav ...");
  size_t wavSize = 0;
  uint8_t *wavBuffer = gI2S.recordWAV(3, &wavSize);
  gI2S.end();

  if (wavBuffer == nullptr || wavSize == 0) {
    Serial.println("Record failed");
    return;
  }

  File wav = SD_MMC.open("/allinone_mic.wav", FILE_WRITE);
  if (!wav) {
    Serial.println("Failed to open /allinone_mic.wav");
    free(wavBuffer);
    return;
  }

  size_t written = wav.write(wavBuffer, wavSize);
  wav.close();
  free(wavBuffer);

  Serial.printf("WAV write: %u/%u bytes\n", (unsigned int)written, (unsigned int)wavSize);
}

static void test8ImuAndMag() {
  printBanner("[8] IMU + Magnetometer");
  ensureI2C();

  if (gImu.begin_I2C()) {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    gImu.getEvent(&accel, &gyro, &temp);

    Serial.printf("IMU temp=%.2f C\n", temp.temperature);
    Serial.printf("IMU accel x=%.2f y=%.2f z=%.2f m/s^2\n",
                  accel.acceleration.x,
                  accel.acceleration.y,
                  accel.acceleration.z);
    Serial.printf("IMU gyro  x=%.2f y=%.2f z=%.2f rad/s\n",
                  gyro.gyro.x,
                  gyro.gyro.y,
                  gyro.gyro.z);
  } else {
    Serial.println("IMU not found");
  }

  if (gMag.begin()) {
    sensors_event_t magEvent;
    gMag.getEvent(&magEvent);
    Serial.printf("MAG x=%.2f y=%.2f z=%.2f uT\n",
                  magEvent.magnetic.x,
                  magEvent.magnetic.y,
                  magEvent.magnetic.z);
  } else {
    Serial.println("Magnetometer not found");
  }
}

static void test9Rtc() {
  printBanner("[9] RTC");
  ensureI2C();

  gRtc.begin();

  // Set a known baseline time and read it back.
  gRtc.t.year = 2026;
  gRtc.t.month = 4;
  gRtc.t.day = 18;
  gRtc.t.hour = 12;
  gRtc.t.minute = 0;
  gRtc.t.second = 0;
  gRtc.t.dayOfWeek = 6;

  if (!gRtc.writeTime()) {
    Serial.println("RTC writeTime failed");
    return;
  }

  if (!gRtc.readTime()) {
    Serial.println("RTC readTime failed");
    return;
  }

  Serial.printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                gRtc.t.year, gRtc.t.month, gRtc.t.day,
                gRtc.t.hour, gRtc.t.minute, gRtc.t.second);
}

static void test10Power() {
  printBanner("[10] Power (PMIC + Fuel Gauge)");
  ensureI2C();

  gPmic.begin();
  delay(100);

  if (gPmic.isConnected()) {
    Serial.println("PMIC BQ25896 connected");
    Serial.printf("VBUSV=%d mV, BATV=%d mV, SYSV=%d mV\n",
                  gPmic.getVBUSV(),
                  gPmic.getBATV(),
                  gPmic.getSYSV());
    Serial.printf("ICHGR=%d mA, IINLIM=%d mA\n",
                  gPmic.getICHGR(),
                  gPmic.getIINLIM());
    gPmic.setCONV_START(true);
  } else {
    Serial.println("PMIC BQ25896 not found");
  }

  if (gGauge.begin()) {
    int soc = gGauge.readStateOfChargePercent();
    int mv = gGauge.readVoltageMillivolts();
    int ma = gGauge.readCurrentMilliamps();
    float tC = gGauge.readTemperatureCelsius();

    Serial.printf("BQ27220 SOC=%d%% V=%d mV I=%d mA T=%.1f C\n", soc, mv, ma, tC);
    if (ma > 0) {
      Serial.printf("BQ27220 TTF=%d min\n", gGauge.readTimeToFullMinutes());
    }
  } else {
    Serial.println("BQ27220 not found");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupDisplayIfNeeded();
  if (gGfx != nullptr) {
    gGfx->fillScreen(0x0000);
    gScreenLogLine = 0;
    screenLog("KODE all-in-one test");
  }

  printBanner("KODE all-in-one test start");
  screenLog("Start all tests");

  test1Esp32Info();
  screenLog("[DONE] 1 ESP32 info");
  test2IoExpander();
  screenLog("[DONE] 2 IO expander");
  test4RgbLed();
  screenLog("[DONE] 4 RGB LED");
  test5Buttons();
  screenLog("[DONE] 5 Buttons");
  test6MicroSd();
  screenLog("[DONE] 6 microSD");
  test7Audio();
  screenLog("[DONE] 7 Audio");
  test3DisplayAndLvgl();
  screenLog("[DONE] 3 Display+LVGL");
  test8ImuAndMag();
  screenLog("[DONE] 8 IMU+MAG");
  test9Rtc();
  screenLog("[DONE] 9 RTC");
  test10Power();
  screenLog("[DONE] 10 Power");

  printBanner("All tests finished");
  Serial.println("Loop keeps LVGL alive and prints heartbeat every 5s.");
  screenLog("All tests finished");
}

void loop() {
  static uint32_t lastHeartbeat = 0;

  if (gLvglReady) {
    lv_timer_handler();
  }

  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    logBoth("all-in-one running...");
  }

  delay(5);
}
