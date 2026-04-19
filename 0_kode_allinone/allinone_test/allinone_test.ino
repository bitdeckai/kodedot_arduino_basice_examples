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
#include <string.h>

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
static const int TOUCH_OFFSET_X = 0;
static const int TOUCH_OFFSET_Y = -40;

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
static bool gI2cReady = false;
static bool gLvglReady = false;
static bool gTouchReady = false;
static bool gSdMounted = false;
static bool gExpanderReady = false;
static bool gMenuKeysReady = false;
static uint16_t gExpanderOutput = 0x0000;
static uint16_t gExpanderConfig = 0xFFFF;
static int gScreenLogLine = 0;
static const int SCREEN_LOG_LINE_H = 18;
static const int SCREEN_LOG_MARGIN = 8;
static const int UI_SAFE_MARGIN_X = 28;
static const int UI_SAFE_MARGIN_Y = 34;
static const int UI_LOG_BUFFER_SIZE = 4096;
static const bool FORCE_EXP4_LOW_TEST = false;
static const int UI_SCALE_MENU_TITLE = 384;
static const int UI_SCALE_MENU_BUTTON_TEXT = 320;
static const int UI_SCALE_RUN_TITLE = 256;
static const int UI_SCALE_RUN_STATUS = 256;
static const int UI_SCALE_RUN_LOG = 256;

typedef void (*TestMenuRunner)();

struct TestMenuEntry {
  const char *label;
  TestMenuRunner runner;
};

static void test1Esp32Info();
static void test2IoExpander();
static void test4RgbLed();
static void test5Buttons();
static void test6MicroSd();
static void test7Audio();
static void test8ImuAndMag();
static void test9Rtc();
static void test10Power();

static const TestMenuEntry kTestMenuEntries[] = {
  {"[1] ESP32 Info", test1Esp32Info},
  {"[2] IO Expander", test2IoExpander},
  {"[3] RGB LED", test4RgbLed},
  {"[4] Buttons", test5Buttons},
  {"[5] microSD", test6MicroSd},
  {"[6] Audio", test7Audio},
  {"[7] IMU + MAG", test8ImuAndMag},
  {"[8] RTC", test9Rtc},
  {"[9] Power", test10Power},
};

static lv_obj_t *gMenuScreen = nullptr;
static lv_obj_t *gMenuPanel = nullptr;
static lv_obj_t *gMenuButtons[10] = {nullptr};
static lv_obj_t *gRunScreen = nullptr;
static lv_obj_t *gRunTitle = nullptr;
static lv_obj_t *gRunStatus = nullptr;
static lv_obj_t *gRunLogPanel = nullptr;
static lv_obj_t *gRunLogLabel = nullptr;
static lv_obj_t *gBackButton = nullptr;
static lv_obj_t *gBackLabel = nullptr;
static int gMenuSelectedIndex = 0;
static int gPendingTestIndex = -1;
static bool gTestRunning = false;
static char gRunLogText[UI_LOG_BUFFER_SIZE] = {0};
static uint32_t gLastMenuNavMs = 0;
static bool gMenuUpPrev = false;
static bool gMenuDownPrev = false;
static bool gMenuSelectPrev = false;
static bool gMenuBackPrev = false;
static uint32_t gSingleButtonPressStartMs = 0;
static bool gSingleButtonLongHandled = false;
static bool gSingleButtonModeLogged = false;
static bool gMenuInputArmed = false;
static uint32_t gMenuInputReleaseSinceMs = 0;

static bool setupLvglIfNeeded();
static void showMainMenu();
static void showRunScreen(int testIndex, bool completed);
static void menuButtonEvent(lv_event_t *e);
static void backButtonEvent(lv_event_t *e);
static void runPendingMenuTest();
static bool setupMenuKeysIfNeeded();
static void refreshMenuSelection();
static void resetMenuInputLatch();
static void handleMenuInputFallback();
static const lv_font_t *uiTitleFont();
static const lv_font_t *uiBodyFont();
static void clearRunLog();
static void appendRunLogLine(const char *msg);

static size_t menuTestCount() {
  return sizeof(kTestMenuEntries) / sizeof(kTestMenuEntries[0]);
}

static const lv_font_t *uiTitleFont() {
  return LV_FONT_DEFAULT;
}

static const lv_font_t *uiBodyFont() {
  return LV_FONT_DEFAULT;
}

static int clampPercent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

static int absoluteInt(int value) {
  return value < 0 ? -value : value;
}

static int estimateBatteryPercentFromVoltage(int millivolts) {
  struct BatterySocPoint {
    int millivolts;
    int percent;
  };

  static const BatterySocPoint kCurve[] = {
    {3300, 0},
    {3600, 10},
    {3700, 20},
    {3750, 35},
    {3800, 50},
    {3900, 70},
    {4000, 85},
    {4100, 95},
    {4200, 100},
  };

  if (millivolts <= kCurve[0].millivolts) {
    return kCurve[0].percent;
  }

  for (size_t i = 1; i < sizeof(kCurve) / sizeof(kCurve[0]); ++i) {
    if (millivolts <= kCurve[i].millivolts) {
      int mv0 = kCurve[i - 1].millivolts;
      int mv1 = kCurve[i].millivolts;
      int soc0 = kCurve[i - 1].percent;
      int soc1 = kCurve[i].percent;
      long numerator = (long)(millivolts - mv0) * (soc1 - soc0);
      return clampPercent(soc0 + (int)(numerator / (mv1 - mv0)));
    }
  }

  return 100;
}

static void clearRunLog() {
  gRunLogText[0] = '\0';
  gScreenLogLine = 0;
  if (gRunLogLabel != nullptr) {
    lv_label_set_text(gRunLogLabel, "");
  }
}

static void appendRunLogLine(const char *msg) {
  if (msg == nullptr || gRunLogLabel == nullptr) {
    return;
  }

  size_t currentLen = strlen(gRunLogText);
  size_t msgLen = strlen(msg);
  size_t needed = currentLen + msgLen + (currentLen > 0 ? 1 : 0) + 1;
  if (needed >= UI_LOG_BUFFER_SIZE) {
    const char *overflow = "...\n";
    size_t overflowLen = strlen(overflow);
    memmove(gRunLogText, gRunLogText + (UI_LOG_BUFFER_SIZE / 3), currentLen - (UI_LOG_BUFFER_SIZE / 3) + 1);
    currentLen = strlen(gRunLogText);
    if (currentLen + overflowLen + 1 < UI_LOG_BUFFER_SIZE) {
      strcat(gRunLogText, overflow);
      currentLen = strlen(gRunLogText);
    }
  }

  if (currentLen > 0 && currentLen + 1 < UI_LOG_BUFFER_SIZE) {
    gRunLogText[currentLen++] = '\n';
    gRunLogText[currentLen] = '\0';
  }
  strncat(gRunLogText, msg, UI_LOG_BUFFER_SIZE - strlen(gRunLogText) - 1);
  lv_label_set_text(gRunLogLabel, gRunLogText);
  lv_obj_scroll_to_y(gRunLogPanel, LV_COORD_MAX, LV_ANIM_OFF);
}

static void refreshMenuSelection() {
  for (size_t i = 0; i < menuTestCount(); ++i) {
    lv_obj_t *btn = gMenuButtons[i];
    if (btn == nullptr) {
      continue;
    }

    if ((int)i == gMenuSelectedIndex) {
      lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
      lv_obj_set_style_border_color(btn, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_shadow_width(btn, 16, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_TEAL, 3), LV_PART_MAIN);
      lv_obj_set_style_border_color(btn, lv_palette_lighten(LV_PALETTE_AMBER, 2), LV_PART_MAIN);
      lv_obj_set_style_shadow_width(btn, 10, LV_PART_MAIN);
    }
  }

  if (gMenuSelectedIndex >= 0 && gMenuSelectedIndex < (int)menuTestCount() && gMenuButtons[gMenuSelectedIndex] != nullptr) {
    lv_obj_scroll_to_view(gMenuButtons[gMenuSelectedIndex], LV_ANIM_OFF);
  }
}

static void resetMenuInputLatch() {
  gMenuInputArmed = false;
  gMenuInputReleaseSinceMs = 0;
  gMenuUpPrev = false;
  gMenuDownPrev = false;
  gMenuSelectPrev = false;
  gMenuBackPrev = false;
  gSingleButtonPressStartMs = 0;
  gSingleButtonLongHandled = false;
  gLastMenuNavMs = millis();
}

static bool setupMenuKeysIfNeeded() {
  if (gMenuKeysReady) {
    return true;
  }

  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);

  if (setupExpander()) {
    expanderPinMode(6, INPUT);
    expanderPinMode(7, INPUT);
    expanderPinMode(8, INPUT);
    expanderPinMode(9, INPUT);
    expanderPinMode(11, INPUT);
    Serial.println("Menu key fallback ready");
  } else {
    Serial.println("Menu key fallback limited: expander unavailable");
  }

  gMenuKeysReady = true;
  return true;
}

static void handleMenuInputFallback() {
  setupMenuKeysIfNeeded();

  bool gpioButtonNow = digitalRead(PIN_BUTTON_UP) == LOW;
  bool allReleased = !gpioButtonNow;
  if (gExpanderReady) {
    allReleased = allReleased &&
                  expanderDigitalRead(6) != LOW &&
                  expanderDigitalRead(7) != LOW &&
                  expanderDigitalRead(8) != LOW &&
                  expanderDigitalRead(9) != LOW &&
                  expanderDigitalRead(11) != LOW;
  }

  if (!gMenuInputArmed) {
    if (allReleased) {
      if (gMenuInputReleaseSinceMs == 0) {
        gMenuInputReleaseSinceMs = millis();
      } else if (millis() - gMenuInputReleaseSinceMs >= 250) {
        gMenuInputArmed = true;
      }
    } else {
      gMenuInputReleaseSinceMs = 0;
    }

    gMenuUpPrev = gpioButtonNow;
    gMenuDownPrev = false;
    gMenuSelectPrev = false;
    gMenuBackPrev = false;
    return;
  }

  if (!gExpanderReady) {
    if (!gSingleButtonModeLogged) {
      Serial.println("Single-button menu mode: short press=next, long press=enter/back");
      gSingleButtonModeLogged = true;
    }

    if (gpioButtonNow && !gMenuUpPrev) {
      gSingleButtonPressStartMs = millis();
      gSingleButtonLongHandled = false;
    }

    if (gpioButtonNow && !gSingleButtonLongHandled && millis() - gSingleButtonPressStartMs >= 700) {
      gSingleButtonLongHandled = true;
      if (!gTestRunning && lv_screen_active() == gMenuScreen) {
        gPendingTestIndex = gMenuSelectedIndex;
      } else if (!gTestRunning && lv_screen_active() == gRunScreen) {
        if (gBackButton != nullptr && !lv_obj_has_flag(gBackButton, LV_OBJ_FLAG_HIDDEN)) {
          showMainMenu();
        }
      }
      gLastMenuNavMs = millis();
    }

    if (!gpioButtonNow && gMenuUpPrev) {
      uint32_t pressMs = millis() - gSingleButtonPressStartMs;
      if (!gSingleButtonLongHandled && pressMs >= 30) {
        if (!gTestRunning && lv_screen_active() == gMenuScreen) {
          gMenuSelectedIndex = (gMenuSelectedIndex + 1) % (int)menuTestCount();
          refreshMenuSelection();
          gLastMenuNavMs = millis();
        }
      }
    }

    gMenuUpPrev = gpioButtonNow;
    gMenuDownPrev = false;
    gMenuSelectPrev = false;
    gMenuBackPrev = false;
    return;
  }

  gSingleButtonModeLogged = false;

  bool upNow = gpioButtonNow || expanderDigitalRead(6) == LOW;
  bool downNow = gExpanderReady && expanderDigitalRead(8) == LOW;
  bool selectNow = gExpanderReady && (expanderDigitalRead(9) == LOW || expanderDigitalRead(11) == LOW);
  bool backNow = gExpanderReady && expanderDigitalRead(7) == LOW;

  bool canTrigger = millis() - gLastMenuNavMs >= 160;

  if (!gTestRunning && lv_screen_active() == gMenuScreen) {
    if (canTrigger && upNow && !gMenuUpPrev) {
      gMenuSelectedIndex = (gMenuSelectedIndex - 1 + (int)menuTestCount()) % (int)menuTestCount();
      refreshMenuSelection();
      gLastMenuNavMs = millis();
    } else if (canTrigger && downNow && !gMenuDownPrev) {
      gMenuSelectedIndex = (gMenuSelectedIndex + 1) % (int)menuTestCount();
      refreshMenuSelection();
      gLastMenuNavMs = millis();
    } else if (canTrigger && selectNow && !gMenuSelectPrev) {
      gPendingTestIndex = gMenuSelectedIndex;
      gLastMenuNavMs = millis();
    }
  } else if (!gTestRunning && lv_screen_active() == gRunScreen) {
    if (gBackButton != nullptr && !lv_obj_has_flag(gBackButton, LV_OBJ_FLAG_HIDDEN)) {
      if (canTrigger && ((selectNow && !gMenuSelectPrev) || (backNow && !gMenuBackPrev) || (upNow && !gMenuUpPrev))) {
        showMainMenu();
        gLastMenuNavMs = millis();
      }
    }
  }

  gMenuUpPrev = upNow;
  gMenuDownPrev = downNow;
  gMenuSelectPrev = selectNow;
  gMenuBackPrev = backNow;
}

static void screenLog(const char *msg) {
  if (gTestRunning && gRunLogLabel != nullptr) {
    appendRunLogLine(msg);
    return;
  }

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

static bool setupLvglIfNeeded() {
  if (gLvglReady) {
    return true;
  }

  if (!gExpanderReady) {
    if (setupExpander()) {
      Serial.println("Peripheral rail enabled before LVGL/touch init");
      delay(50);
    }
  }

  if (!setupDisplayIfNeeded()) {
    return false;
  }

  lv_init();
  lv_tick_set_cb(lvTickMs);

  if (gTouch.init(PIN_I2C_SDA, PIN_I2C_SCL, -1, -1, 400000) == CT_SUCCESS) {
    gTouch.setOrientation(0, DSP_HOR_RES, DSP_VER_RES);
    gI2cReady = true;
    gTouchReady = true;
    Serial.printf("Touch initialized, type=%d\n", gTouch.sensorType());
  } else {
    gTouchReady = false;
    Serial.println("Touch init failed, buttons fallback active");
  }

  gLvDisplay = lv_display_create(DSP_HOR_RES, DSP_VER_RES);
  lv_display_set_flush_cb(gLvDisplay, lvglFlushCb);

  size_t drawBuf = (DSP_HOR_RES * DSP_VER_RES / 10) * (LV_COLOR_DEPTH / 8);
  gLvBuf1 = (uint8_t *)heap_caps_malloc(drawBuf, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  gLvBuf2 = (uint8_t *)heap_caps_malloc(drawBuf, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (gLvBuf1 == nullptr || gLvBuf2 == nullptr) {
    Serial.println("LVGL buffer allocation failed");
    return false;
  }

  lv_display_set_buffers(gLvDisplay, gLvBuf1, gLvBuf2, drawBuf, LV_DISPLAY_RENDER_MODE_PARTIAL);

  gLvIndev = lv_indev_create();
  lv_indev_set_type(gLvIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(gLvIndev, lvglTouchReadCb);

  gLvglReady = true;
  Serial.println("LVGL initialized");
  return true;
}

static void menuButtonEvent(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED || gTestRunning) {
    return;
  }

  gMenuSelectedIndex = (int)(intptr_t)lv_event_get_user_data(e);
  refreshMenuSelection();
  gPendingTestIndex = gMenuSelectedIndex;
}

static void backButtonEvent(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED || gTestRunning) {
    return;
  }

  showMainMenu();
}

static void showRunScreen(int testIndex, bool completed) {
  if (!setupLvglIfNeeded()) {
    return;
  }

  if (gRunScreen == nullptr) {
    gRunScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(gRunScreen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gRunScreen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(gRunScreen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gRunScreen, 0, LV_PART_MAIN);

    gRunTitle = lv_label_create(gRunScreen);
    lv_obj_set_width(gRunTitle, DSP_HOR_RES - UI_SAFE_MARGIN_X * 2);
    lv_obj_set_pos(gRunTitle, UI_SAFE_MARGIN_X, UI_SAFE_MARGIN_Y);
    lv_obj_set_style_text_color(gRunTitle, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(gRunTitle, uiTitleFont(), LV_PART_MAIN);
    lv_obj_set_style_text_align(gRunTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(gRunTitle, (DSP_HOR_RES - UI_SAFE_MARGIN_X * 2) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(gRunTitle, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(gRunTitle, UI_SCALE_RUN_TITLE, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(gRunTitle, UI_SCALE_RUN_TITLE, LV_PART_MAIN);

    gRunStatus = lv_label_create(gRunScreen);
    lv_obj_set_width(gRunStatus, DSP_HOR_RES - UI_SAFE_MARGIN_X * 2);
    lv_obj_set_pos(gRunStatus, UI_SAFE_MARGIN_X, UI_SAFE_MARGIN_Y + 76);
    lv_obj_set_style_text_color(gRunStatus, lv_palette_main(LV_PALETTE_AMBER), LV_PART_MAIN);
    lv_obj_set_style_text_font(gRunStatus, uiBodyFont(), LV_PART_MAIN);
    lv_obj_set_style_text_align(gRunStatus, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(gRunStatus, UI_SCALE_RUN_STATUS, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(gRunStatus, UI_SCALE_RUN_STATUS, LV_PART_MAIN);

    gRunLogPanel = lv_obj_create(gRunScreen);
    lv_obj_set_size(gRunLogPanel,
                    DSP_HOR_RES - UI_SAFE_MARGIN_X * 2,
            DSP_VER_RES - UI_SAFE_MARGIN_Y * 2 - 206);
    lv_obj_set_pos(gRunLogPanel, UI_SAFE_MARGIN_X, UI_SAFE_MARGIN_Y + 140);
    lv_obj_set_style_bg_color(gRunLogPanel, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN);
    lv_obj_set_style_border_color(gRunLogPanel, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(gRunLogPanel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(gRunLogPanel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gRunLogPanel, 16, LV_PART_MAIN);
    lv_obj_set_scroll_dir(gRunLogPanel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(gRunLogPanel, LV_SCROLLBAR_MODE_ACTIVE);

    gRunLogLabel = lv_label_create(gRunLogPanel);
    lv_obj_set_width(gRunLogLabel, DSP_HOR_RES - UI_SAFE_MARGIN_X * 2 - 36);
    lv_label_set_long_mode(gRunLogLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(gRunLogLabel, lv_palette_lighten(LV_PALETTE_ORANGE, 4), LV_PART_MAIN);
    lv_obj_set_style_text_font(gRunLogLabel, uiBodyFont(), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(gRunLogLabel, 10, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(gRunLogLabel, UI_SCALE_RUN_LOG, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(gRunLogLabel, UI_SCALE_RUN_LOG, LV_PART_MAIN);

    gBackButton = lv_btn_create(gRunScreen);
    lv_obj_set_size(gBackButton, 210, 64);
    lv_obj_align(gBackButton, LV_ALIGN_BOTTOM_MID, 0, -UI_SAFE_MARGIN_Y);
    lv_obj_set_style_bg_color(gBackButton, lv_palette_main(LV_PALETTE_DEEP_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(gBackButton, lv_palette_darken(LV_PALETTE_DEEP_ORANGE, 2), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(gBackButton, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(gBackButton, 18, LV_PART_MAIN);
    lv_obj_add_event_cb(gBackButton, backButtonEvent, LV_EVENT_CLICKED, nullptr);

    gBackLabel = lv_label_create(gBackButton);
    lv_label_set_text(gBackLabel, "Back");
    lv_obj_set_style_text_color(gBackLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(gBackLabel, uiBodyFont(), LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(gBackLabel, UI_SCALE_MENU_BUTTON_TEXT, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(gBackLabel, UI_SCALE_MENU_BUTTON_TEXT, LV_PART_MAIN);
    lv_obj_center(gBackLabel);
  }

  const char *title = (testIndex >= 0 && testIndex < (int)menuTestCount())
    ? kTestMenuEntries[testIndex].label
    : "Test";
  lv_label_set_text(gRunTitle, title);
  lv_label_set_text(gRunStatus, completed ? "Test finished. Tap Back." : "Testing... please wait.");
  if (!completed) {
    clearRunLog();
  }

  if (completed) {
    lv_obj_clear_flag(gBackButton, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(gBackButton, LV_OBJ_FLAG_HIDDEN);
  }

  lv_screen_load(gRunScreen);
  lv_timer_handler();
}

static void showMainMenu() {
  if (!setupLvglIfNeeded()) {
    return;
  }

  if (gMenuScreen == nullptr) {
    gMenuScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(gMenuScreen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gMenuScreen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(gMenuScreen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gMenuScreen, 0, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(gMenuScreen);
    lv_label_set_text(title, "KODE Test Menu");
    lv_obj_set_width(title, DSP_HOR_RES - UI_SAFE_MARGIN_X * 2);
    lv_obj_set_style_text_color(title, lv_palette_lighten(LV_PALETTE_ORANGE, 3), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, uiTitleFont(), LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(title, (DSP_HOR_RES - UI_SAFE_MARGIN_X * 2) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(title, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(title, UI_SCALE_MENU_TITLE, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(title, UI_SCALE_MENU_TITLE, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_SAFE_MARGIN_Y - 8);

    gMenuPanel = lv_obj_create(gMenuScreen);
    lv_obj_set_size(gMenuPanel,
                    DSP_HOR_RES - UI_SAFE_MARGIN_X * 2,
                    DSP_VER_RES - UI_SAFE_MARGIN_Y * 2 - 18);
    lv_obj_set_pos(gMenuPanel, UI_SAFE_MARGIN_X, UI_SAFE_MARGIN_Y + 28);
    lv_obj_set_style_bg_color(gMenuPanel, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gMenuPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(gMenuPanel, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(gMenuPanel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(gMenuPanel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gMenuPanel, 12, LV_PART_MAIN);
    lv_obj_set_scroll_dir(gMenuPanel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(gMenuPanel, LV_SCROLLBAR_MODE_ACTIVE);

    const lv_coord_t btnW = DSP_HOR_RES - UI_SAFE_MARGIN_X * 2 - 24;
    const lv_coord_t btnH = 72;
    const lv_coord_t gapY = 14;

    for (size_t i = 0; i < menuTestCount(); ++i) {
      lv_obj_t *btn = lv_btn_create(gMenuPanel);
      gMenuButtons[i] = btn;
      lv_obj_set_size(btn, btnW, btnH);
      lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 10 + (lv_coord_t)i * (btnH + gapY));
      lv_obj_set_style_radius(btn, 20, LV_PART_MAIN);
      lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_TEAL, 3), LV_PART_MAIN);
      lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_TEAL), LV_STATE_PRESSED);
      lv_obj_set_style_border_color(btn, lv_palette_lighten(LV_PALETTE_AMBER, 2), LV_PART_MAIN);
      lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
      lv_obj_set_style_shadow_width(btn, 10, LV_PART_MAIN);
      lv_obj_set_style_shadow_color(btn, lv_palette_darken(LV_PALETTE_TEAL, 4), LV_PART_MAIN);
      lv_obj_add_event_cb(btn, menuButtonEvent, LV_EVENT_CLICKED, (void *)(intptr_t)i);

      lv_obj_t *label = lv_label_create(btn);
      lv_label_set_text(label, kTestMenuEntries[i].label);
  lv_obj_set_width(label, btnW - 32);
      lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(label, uiBodyFont(), LV_PART_MAIN);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_x(label, (btnW - 32) / 2, LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_y(label, 0, LV_PART_MAIN);
      lv_obj_set_style_transform_scale_x(label, UI_SCALE_MENU_BUTTON_TEXT, LV_PART_MAIN);
      lv_obj_set_style_transform_scale_y(label, UI_SCALE_MENU_BUTTON_TEXT, LV_PART_MAIN);
      lv_obj_center(label);
    }

    refreshMenuSelection();
  }

  refreshMenuSelection();
  resetMenuInputLatch();
  gPendingTestIndex = -1;
  lv_screen_load(gMenuScreen);
  lv_timer_handler();
}

static void runPendingMenuTest() {
  int testIndex = gPendingTestIndex;
  if (testIndex < 0 || testIndex >= (int)menuTestCount()) {
    gPendingTestIndex = -1;
    return;
  }

  gPendingTestIndex = -1;
  gTestRunning = true;

  if (gGfx != nullptr) {
    gGfx->fillScreen(0x0000);
  }
  showRunScreen(testIndex, false);
  printBanner(kTestMenuEntries[testIndex].label);
  kTestMenuEntries[testIndex].runner();
  showRunScreen(testIndex, true);

  gTestRunning = false;
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
  if (!gTouchReady) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  TOUCHINFO ti;
  if (gTouch.getSamples(&ti) && ti.count > 0) {
    int touchX = ti.x[0] + TOUCH_OFFSET_X;
    int touchY = ti.y[0] + TOUCH_OFFSET_Y;
    if (touchX < 0) touchX = 0;
    if (touchY < 0) touchY = 0;
    if (touchX >= DSP_HOR_RES) touchX = DSP_HOR_RES - 1;
    if (touchY >= DSP_VER_RES) touchY = DSP_VER_RES - 1;

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touchX;
    data->point.y = touchY;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void ensureI2C() {
  if (!gI2cReady) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    gI2cReady = true;
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
      !expanderDigitalWrite(3, HIGH) ||
      !expanderDigitalWrite(4, FORCE_EXP4_LOW_TEST ? LOW : HIGH)) {
    Serial.println("Expander power pin setup failed");
    return false;
  }

  gExpanderReady = true;

  Serial.println("Expander initialized (0x20)");
  if (FORCE_EXP4_LOW_TEST) {
    Serial.println("TEST mode: EXP4 forced LOW");
  }
  Serial.printf("EXP3 cfg=%u out=%u in=%d | EXP4 cfg=%u out=%u in=%d\n",
                (unsigned int)((gExpanderConfig >> 3) & 0x1),
                (unsigned int)((gExpanderOutput >> 3) & 0x1),
                expanderDigitalRead(3),
                (unsigned int)((gExpanderConfig >> 4) & 0x1),
                (unsigned int)((gExpanderOutput >> 4) & 0x1),
                expanderDigitalRead(4));
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
  logBothf("ESP32 model: %s rev %d", ESP.getChipModel(), ESP.getChipRevision());
  logBothf("CPU cores: %d", ESP.getChipCores());
  logBothf("Chip ID: %llu", ESP.getEfuseMac());
}

static void test2IoExpander() {
  printBanner("[2] IO Expander");
  if (!setupExpander()) {
    logBoth("SKIP: expander not ready");
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
    logBothf("EXP%02d=%d %s", pins[i], level, names[i]);
  }
}

static bool setupDisplayIfNeeded() {
  if (gGfx != nullptr) {
    return true;
  }

  if (!gExpanderReady) {
    if (setupExpander()) {
      Serial.println("Peripheral rail enabled before display init");
      delay(50);
    }
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

  pinMode(DSP_RST, OUTPUT);
  digitalWrite(DSP_RST, LOW);
  delay(10);
  digitalWrite(DSP_RST, HIGH);
  delay(20);

  Serial.println("Display initialized");
  return true;
}

static void test4RgbLed() {
  printBanner("[3] Addressable LED");

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
  printBanner("[4] Buttons");
  if (!setupExpander()) {
    logBoth("SKIP: expander not ready");
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

  logBoth("Press buttons now (3 seconds)...");
  uint32_t start = millis();
  while (millis() - start < TEST_WINDOW_MS) {
    if (gButtonUpInterrupted) {
      gButtonUpInterrupted = false;
      logBoth("BUTTON_UP pressed");
    }

    if (gExpanderInterrupted) {
      gExpanderInterrupted = false;
      if (expanderDigitalRead(6) == LOW) logBoth("PAD_UP pressed");
      if (expanderDigitalRead(7) == LOW) logBoth("PAD_LEFT pressed");
      if (expanderDigitalRead(8) == LOW) logBoth("PAD_DOWN pressed");
      if (expanderDigitalRead(9) == LOW) logBoth("BUTTON_BOTTOM pressed");
      if (expanderDigitalRead(11) == LOW) logBoth("PAD_RIGHT pressed");
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
  printBanner("[5] microSD");
  if (!mountSdIfNeeded()) {
    logBoth("SKIP: SD card not available");
    return;
  }

  File f = SD_MMC.open("/allinone.txt", FILE_WRITE);
  if (!f) {
    logBoth("Open /allinone.txt failed");
    return;
  }
  f.print("all-in-one test\n");
  f.close();

  f = SD_MMC.open("/allinone.txt");
  if (!f) {
    logBoth("Read /allinone.txt failed");
    return;
  }

  Serial.print("/allinone.txt: ");
  while (f.available()) {
    Serial.write(f.read());
  }
  Serial.println();
  f.close();

  logBothf("SD total=%lluMB used=%lluMB",
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
  printBanner("[6] Audio (speaker + microphone)");

  if (!setupExpander()) {
    logBoth("Expander missing, speaker amplifier may stay disabled");
  } else {
    expanderPinMode(3, OUTPUT);
    expanderDigitalWrite(3, HIGH);
  }

  if (!beginI2STx()) {
    logBoth("SKIP: I2S TX init failed");
  } else {
    const int frequency = 300;
    int32_t sample = 500;
    int count = 0;

    logBoth("Playing 2s square wave...");
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
    logBoth("SKIP: I2S monitor init failed");
  } else {
    logBoth("Live monitor: microphone to speaker (3s)...");
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
    logBothf("Live monitor finished, frames=%u", (unsigned int)frames);
  }

  if (!mountSdIfNeeded()) {
    logBoth("SKIP: audio record (no SD card)");
    return;
  }

  if (!beginI2SRx()) {
    logBoth("SKIP: I2S RX init failed");
    return;
  }

  logBoth("Recording 3s to /allinone_mic.wav ...");
  size_t wavSize = 0;
  uint8_t *wavBuffer = gI2S.recordWAV(3, &wavSize);
  gI2S.end();

  if (wavBuffer == nullptr || wavSize == 0) {
    logBoth("Record failed");
    return;
  }

  File wav = SD_MMC.open("/allinone_mic.wav", FILE_WRITE);
  if (!wav) {
    logBoth("Failed to open /allinone_mic.wav");
    free(wavBuffer);
    return;
  }

  size_t written = wav.write(wavBuffer, wavSize);
  wav.close();
  free(wavBuffer);

  logBothf("WAV write: %u/%u bytes", (unsigned int)written, (unsigned int)wavSize);
}

static void test8ImuAndMag() {
  printBanner("[7] IMU + Magnetometer");
  ensureI2C();

  if (gImu.begin_I2C()) {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    gImu.getEvent(&accel, &gyro, &temp);

    logBothf("IMU temp=%.2f C", temp.temperature);
    logBothf("ACC x=%.2f y=%.2f z=%.2f",
                  accel.acceleration.x,
                  accel.acceleration.y,
                  accel.acceleration.z);
    logBothf("GYR x=%.2f y=%.2f z=%.2f",
                  gyro.gyro.x,
                  gyro.gyro.y,
                  gyro.gyro.z);
  } else {
    logBoth("IMU not found");
  }

  if (gMag.begin()) {
    sensors_event_t magEvent;
    gMag.getEvent(&magEvent);
    logBothf("MAG x=%.2f y=%.2f z=%.2f uT",
                  magEvent.magnetic.x,
                  magEvent.magnetic.y,
                  magEvent.magnetic.z);
  } else {
    logBoth("Magnetometer not found");
  }
}

static void test9Rtc() {
  printBanner("[8] RTC");
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
    logBoth("RTC writeTime failed");
    return;
  }

  if (!gRtc.readTime()) {
    logBoth("RTC readTime failed");
    return;
  }

  logBothf("RTC: %04d-%02d-%02d %02d:%02d:%02d",
                gRtc.t.year, gRtc.t.month, gRtc.t.day,
                gRtc.t.hour, gRtc.t.minute, gRtc.t.second);
}

static void test10Power() {
  printBanner("[9] Power (PMIC + Fuel Gauge)");
  ensureI2C();

  gPmic.begin();
  delay(100);

  if (gPmic.isConnected()) {
    logBoth("PMIC BQ25896 connected");
    logBothf("VBUS=%d BAT=%d SYS=%d mV",
                  gPmic.getVBUSV(),
                  gPmic.getBATV(),
                  gPmic.getSYSV());
    logBothf("ICHG=%d IINLIM=%d mA",
                  gPmic.getICHGR(),
                  gPmic.getIINLIM());
    gPmic.setCONV_START(true);
  } else {
    logBoth("PMIC BQ25896 not found");
  }

  if (gGauge.begin(Wire, BQ27220_I2C_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL, 400000U)) {
    int soc = gGauge.readStateOfChargePercent();
    int mv = gGauge.readVoltageMillivolts();
    int ma = gGauge.readCurrentMilliamps();
    int avgMa = gGauge.readAverageCurrentMilliamps();
    float tC = gGauge.readTemperatureCelsius();
    int remainingCapacity = gGauge.readRemainingCapacitymAh();
    int fullChargeCapacity = gGauge.readFullChargeCapacitymAh();
    int designCapacity = gGauge.readDesignCapacitymAh();
    int cycleCount = gGauge.readCycleCount();
    int soh = gGauge.readStateOfHealthPercent();
    int voltageSoc = estimateBatteryPercentFromVoltage(mv);
    int capacitySoc = -1;
    int usableSoc = -1;
    const char *usableSocSource = "unknown";
    uint16_t batteryStatus = 0;
    uint16_t operationStatus = 0;
    bool batteryStatusOk = gGauge.readBatteryStatus(batteryStatus);
    bool operationStatusOk = gGauge.readOperationStatus(operationStatus);
    bool gaugeSocTrusted = true;

    if (remainingCapacity >= 0 && fullChargeCapacity > 0) {
      capacitySoc = clampPercent((remainingCapacity * 100 + fullChargeCapacity / 2) / fullChargeCapacity);
    }

    logBothf("BQ27220 SOC=%d%% V=%d I=%d IAVG=%d T=%.1fC", soc, mv, ma, avgMa, tC);
    logBothf("BQ27220 RM=%d FCC=%d DC=%d mAh", remainingCapacity, fullChargeCapacity, designCapacity);
    logBothf("BQ27220 SOH=%d%% Cycles=%d", soh, cycleCount);
    if (capacitySoc >= 0) {
      logBothf("Gauge cross-check: capSOC=%d%% voltSOC~%d%%", capacitySoc, voltageSoc);
    } else {
      logBothf("Gauge cross-check: voltSOC~%d%%", voltageSoc);
    }
    if (batteryStatusOk) {
      logBothf("BQ27220 BatteryStatus=0x%04X", batteryStatus);
    }
    if (operationStatusOk) {
      logBothf("BQ27220 OperationStatus=0x%04X", operationStatus);
    }

    bool suspicious = false;
    if (designCapacity <= 0 || fullChargeCapacity <= 0) {
      logBoth("WARN: gauge capacity registers look invalid; profile/config may be missing.");
      suspicious = true;
      gaugeSocTrusted = false;
    }
    if (soc < 0 || mv < 0) {
      logBoth("WARN: gauge read failed; fuel-gauge data is not trustworthy.");
      suspicious = true;
      gaugeSocTrusted = false;
    }
    if (!batteryStatusOk || !operationStatusOk) {
      logBoth("WARN: gauge status registers could not be read completely.");
      suspicious = true;
    }
    if (soc == 0 && mv >= 3600) {
      logBoth("WARN: SOC is 0% while battery voltage is well above empty; SOC is not trustworthy.");
      suspicious = true;
      gaugeSocTrusted = false;
    }
    if (remainingCapacity == 0 && mv >= 3600) {
      logBoth("WARN: remaining capacity is 0 mAh at a normal cell voltage; gauge learning/profile is likely wrong.");
      suspicious = true;
      gaugeSocTrusted = false;
    }
    if (capacitySoc >= 0 && soc >= 0 && absoluteInt(capacitySoc - soc) > 20) {
      logBothf("WARN: SOC mismatch, gauge=%d%% capacity=%d%%.", soc, capacitySoc);
      suspicious = true;
      gaugeSocTrusted = false;
    }
    if (soc >= 0 && absoluteInt(voltageSoc - soc) > 35 && mv >= 3500 && mv <= 4100) {
      logBothf("WARN: SOC mismatch, gauge=%d%% voltage-est=%d%%.", soc, voltageSoc);
      suspicious = true;
      gaugeSocTrusted = false;
    }

    if (gaugeSocTrusted && soc >= 0) {
      usableSoc = soc;
      usableSocSource = "gauge";
    } else if (capacitySoc >= 0 && remainingCapacity > 0 && fullChargeCapacity > 0 && absoluteInt(capacitySoc - voltageSoc) <= 20) {
      usableSoc = clampPercent((capacitySoc + voltageSoc) / 2);
      usableSocSource = "capacity+voltage";
    } else {
      usableSoc = voltageSoc;
      usableSocSource = "voltage-fallback";
    }

    if (usableSoc >= 0) {
      logBothf("Usable SOC=%d%% (%s)", usableSoc, usableSocSource);
    }
    if (!suspicious) {
      logBoth("PASS: PMIC and fuel-gauge readings look internally consistent.");
    } else if (gPmic.isConnected()) {
      logBoth("RESULT: PMIC path looks good, but gauge SOC/capacity tracking is not trustworthy yet.");
    }

    if (ma > 0) {
      logBothf("BQ27220 TTF=%d min", gGauge.readTimeToFullMinutes());
    } else if (ma < 0) {
      logBothf("BQ27220 TTE=%d min", gGauge.readTimeToEmptyMinutes());
    }
  } else {
    logBoth("BQ27220 not found");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupDisplayIfNeeded();
  if (gGfx != nullptr) {
    gGfx->fillScreen(0x0000);
    gScreenLogLine = 0;
    screenLog("KODE test menu");
  }

  printBanner("KODE test menu ready");
  showMainMenu();
}

void loop() {
  if (gPendingTestIndex >= 0 && !gTestRunning) {
    runPendingMenuTest();
  }

  if (gLvglReady && !gTouchReady) {
    handleMenuInputFallback();
  }

  if (gLvglReady) {
    lv_timer_handler();
  }

  delay(5);
}
