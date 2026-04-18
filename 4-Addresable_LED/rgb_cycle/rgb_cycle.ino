/**
 * Controls a single NeoPixel connected to GPIO 4, lighting it in red, green, and blue.
 * Each color stays on for half a second, turning off between changes.
 * Uses the Adafruit_NeoPixel library to control RGB LEDs.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <Adafruit_NeoPixel.h> /* Library to control NeoPixel strips and LEDs */

#define NEOPIXEL_PIN   4                  /* GPIO pin where the NeoPixel is connected */
#define NUMPIXELS      1                  /* Number of connected NeoPixels */
#define PIXEL_FORMAT   (NEO_GRB + NEO_KHZ800) /* Color format and data speed */

Adafruit_NeoPixel *pixels;                /* Pointer to the NeoPixel object */

#define DELAYVAL       500                /* Delay time between changes (ms) */

void setup() {
  Serial.begin(115200);                   /* Start serial communication for debugging */

  /* Create the NeoPixel object with the defined parameters */
  pixels = new Adafruit_NeoPixel(NUMPIXELS, NEOPIXEL_PIN, PIXEL_FORMAT);

  pixels->begin();                        /* Initialize the NeoPixel */
  pixels->clear();                        /* Ensure the LED starts off */
  pixels->show();                         /* Apply the change */
}

void loop() {
  /* Turn on red */
  pixels->setPixelColor(0, pixels->Color(150, 0, 0));
  pixels->show();
  delay(DELAYVAL);

  /* Turn off */
  pixels->setPixelColor(0, pixels->Color(0, 0, 0));
  pixels->show();
  delay(DELAYVAL);

  /* Turn on green */
  pixels->setPixelColor(0, pixels->Color(0, 150, 0));
  pixels->show();
  delay(DELAYVAL);

  /* Turn off */
  pixels->setPixelColor(0, pixels->Color(0, 0, 0));
  pixels->show();
  delay(DELAYVAL);

  /* Turn on blue */
  pixels->setPixelColor(0, pixels->Color(0, 0, 150));
  pixels->show();
  delay(DELAYVAL);

  /* Turn off */
  pixels->setPixelColor(0, pixels->Color(0, 0, 0));
  pixels->show();
  delay(DELAYVAL);
}
