/**
 * Simple display demo with Arduino_GFX: initializes the panel and draws “Hello World!”.
 * Uses ESP32-S3 QSPI bus, 410x502 resolution, and max brightness.
 * Renders large text roughly centered on a blue background.
*/
/* ───────── KODE | docs.kode.diy ───────── */

#include <Arduino_GFX_Library.h>

#define DSP_HOR_RES 410
#define DSP_VER_RES 502
#define DSP_SCLK 17
#define DSP_SDIO0 15
#define DSP_SDIO1 14
#define DSP_SDIO2 16
#define DSP_SDIO3 10
#define DSP_RST 8
#define DSP_CS 9

/* Objects to handle the graphics bus and display */
static Arduino_DataBus *gfxBus;
static Arduino_CO5300 *gfx;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Simple Display Demo");

  /* ─── Display configuration ───
     QSPI bus: CS, SCLK, D0, D1, D2, D3 */
  gfxBus = new Arduino_ESP32QSPI(DSP_CS, DSP_SCLK, DSP_SDIO0, DSP_SDIO1, DSP_SDIO2, DSP_SDIO3);
  /* Panel constructor: bus, RST, rotation offset (0), x/y offset (0,0),
     width/height, backlight pin (22), options (0,0,0) */
  //gfx = new Arduino_CO5300(gfxBus, DSP_RST, 0, 0, DSP_HOR_RES, DSP_VER_RES, 22, 0, 0, 0);
  gfx = new Arduino_CO5300(gfxBus, DSP_RST, 0, DSP_HOR_RES, DSP_VER_RES, 22, 0, 0, 0);

  if (!gfx->begin()) {
    Serial.println("Error: display init failed");
    while (true) /* halt on failure */ ;
  }

  gfx->setRotation(0);
  gfx->setBrightness(255);
  gfx->displayOn();
  Serial.println("Display initialized");

  /* Print Hello World! */
  gfx->fillScreen(0x001F);
  gfx->setTextSize(4);
  //gfx->setTextColor(0xFD20);
  gfx->setTextColor(0xFD20);
  gfx->setCursor(65, 250);
  gfx->print("Hello World!");
}

void loop() {
  delay(1000);
}
