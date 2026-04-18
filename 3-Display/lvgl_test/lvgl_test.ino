/**
 * Example of using LVGL with Arduino on kode dot.
 * Sets up the display, touch panel, and draws a simple label.
 * More info: https://docs.lvgl.io/master/integration/framework/arduino.html
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <bb_captouch.h>

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy lvgl/examples to lvgl/src/examples. Similarly for the demos lvgl/demos to lvgl/src/demos.
 *Note that the lv_examples library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 *as the examples and demos are now part of the main LVGL library. */

// #include <examples/lv_examples.h>
// #include <demos/lv_demos.h>

/* Display resolution and rotation */
#define DSP_HOR_RES 410
#define DSP_VER_RES 502
#define DSP_ROTATION LV_DISPLAY_ROTATION_0

/* LVGL draw buffer size */
#define DRAW_BUF_SIZE (DSP_HOR_RES * DSP_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
static uint8_t *lv_buf1;
static uint8_t *lv_buf2;

/* Objects to handle display bus and panel */
static Arduino_DataBus *gfxBus;
static Arduino_CO5300 *gfx;
static BBCapTouch touch;

/* Callback for LVGL to push rendered image to the display */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  /* Diagnostic: inspect first pixel on the very first flush */
  static bool first_flush = true;
  if (first_flush) {
    first_flush = false;
    uint16_t first_px = ((uint16_t *)px_map)[0];
    Serial.printf("first flush: first_px=0x%04X w=%u h=%u\n", first_px, w, h);

    /* draw a small visible square to confirm panel accepts write operations */
    gfx->startWrite();
    gfx->fillRect(0, 0, min(40, DSP_HOR_RES), min(40, DSP_VER_RES), 0xF81F); // magenta
    gfx->endWrite();
    delay(200);
  }

  gfx->startWrite();
  gfx->writeAddrWindow(area->x1, area->y1, w, h);
  gfx->writePixels((uint16_t *)px_map, w * h);
  gfx->endWrite();

  /* Tell LVGL flushing is done */
  lv_display_flush_ready(disp);

  Serial.println("Flush called");
}

/* Read data from the touch panel */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  TOUCHINFO ti;
  if (touch.getSamples(&ti) && ti.count > 0) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = ti.x[0];
    data->point.y = ti.y[0];

    Serial.println(String("Setup done ") +
               data->point.x + "," +
               data->point.y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/* Use Arduino's millis() as LVGL tick source */
static uint32_t my_tick(void) {
  return millis();
}

void setup() {
  Serial.begin(115200);
  Serial.println("LVGL with Arduino on Kode Dot");

  /* ─── Display configuration ─── */
  gfxBus = new Arduino_ESP32QSPI(9, 17, 15, 14, 16, 10);
  //gfx = new Arduino_CO5300(gfxBus, 8, 0, 0, DSP_HOR_RES, DSP_VER_RES, 22, 0, 0, 0);
  gfx = new Arduino_CO5300(gfxBus, 8, 0, DSP_HOR_RES, DSP_VER_RES, 22, 0, 0, 0);
  
  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) delay(1000);
  }
  gfx->setRotation(0);
  gfx->setBrightness(255);
  gfx->displayOn();

  /* Ensure backlight / reset pins are asserted (diagnostic) */
  pinMode(22, OUTPUT);
  digitalWrite(22, HIGH); // force backlight on
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);
  delay(10);
  digitalWrite(8, HIGH);  // toggle reset

  gfx->fillScreen(0x0000);
  Serial.println("Display initialized");

  /* Quick hardware color test to verify panel/backlight */
  gfx->fillScreen(0xF800); delay(300); // red
  gfx->fillScreen(0x07E0); delay(300); // green
  gfx->fillScreen(0x001F); delay(300); // blue
  gfx->fillScreen(0x0000);

  /* ─── Touch panel configuration ─── */
  if (touch.init(48, 47, -1, -1, 400000) == CT_SUCCESS) {
    touch.setOrientation(0, DSP_HOR_RES, DSP_VER_RES);
    Serial.printf("Touch OK. Type=%d\n", touch.sensorType());
  } else {
    Serial.println("Touch initialization failed");
  }

  /* ─── Initialize LVGL ─── */
  lv_init();
  lv_tick_set_cb(my_tick); /* Set tick source */

  /* Print LVGL configuration used at compile time */
  Serial.print("LV_COLOR_DEPTH=");
  Serial.println(LV_COLOR_DEPTH);
#ifdef LV_COLOR_16_SWAP
  Serial.println("LV_COLOR_16_SWAP defined");
#else
  Serial.println("LV_COLOR_16_SWAP NOT defined");
#endif
  Serial.print("sizeof(lv_color_t)="); Serial.println(sizeof(lv_color_t));

  lv_display_t *disp = lv_display_create(DSP_HOR_RES, DSP_VER_RES);
  lv_display_set_flush_cb(disp, my_disp_flush);

  /* Allocate buffers in PSRAM */
  lv_buf1 = (uint8_t *)heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lv_buf2 = (uint8_t *)heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lv_display_set_buffers(disp, lv_buf1, lv_buf2, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

  /* Configure input device as a pointer (touch) */
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  /* *******************
   * Create a simple label 
  ******************** */
  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello Arduino, I'm LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_refr_now(NULL);

  /* *******************
   * Try an example. See all the examples
   *  - Online: https://docs.lvgl.io/master/examples.html
   *  - Source codes: https://github.com/lvgl/lvgl/tree/master/examples
  ******************** */
  //  lv_example_btn_1();

  /* *******************
   * Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMO_WIDGETS
  ******************** */
  //    lv_demo_music();

  // final manual fill removed — LVGL drives the framebuffer now

  lv_timer_handler();   // 强制跑一次

  Serial.println("Setup done");

  Serial.println(LVGL_VERSION_MAJOR);
}

void loop() {
  lv_timer_handler(); /* Let LVGL handle the GUI */
  //delay(5);
}
