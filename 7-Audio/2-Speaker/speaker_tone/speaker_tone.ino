/**
 * Generates a square-wave tone over I2S (48 kHz, 32-bit, mono) and enables the amplifier via an I/O expander.
 * The tone streams continuously through I2S; the expander powers the audio stage.
 * Uses custom pins for I2S and for the TCA95XX_16BIT expander.
*/
/* ───────── KODE | docs.kode.diy ───────── */

#include <ESP_I2S.h>
#include <esp_io_expander.hpp>

/* Expander pin configuration */
#define I2C_SCL_PIN     (47)
#define I2C_SDA_PIN     (48)
#define I2C_ADDR        (0x20)

/* I2S interface pins */
const uint8_t I2S_SCK   = 38;  // Serial clock (SCK)
const uint8_t I2S_WS    = 45;  // Word Select / LRCLK
const uint8_t I2S_DOUT  = 46;  // Data output (SD)

/* Audio signal parameters */
const int frequency   = 300;    // Square wave frequency in Hz
const int amplitude   = 500;    // Square wave amplitude

/* Signal generation state variables */
int32_t sample = amplitude;  // Current sample value
int count = 0;               // Sample counter

/* Global instances */
I2SClass i2s;                // I2S interface object
esp_expander::Base *expander = nullptr;  // Expander instance pointer

void setup() {
  Serial.begin(115200);
  Serial.println("Simple I2S tone");

  /* Configure I2S pins (no MCLK: pass -1) */
  i2s.setPins(I2S_SCK, I2S_WS, I2S_DOUT, -1);

  /* Initialize I2S: standard mode, 48 kHz, 32-bit, mono, left slot */
  if (!i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S!");
    while (1);  // Halt on failure
  }
  Serial.println("I2S bus initialized.");

  /* Initialize the I/O expander */
  expander = new esp_expander::TCA95XX_16BIT(I2C_SCL_PIN, I2C_SDA_PIN, I2C_ADDR);
  expander->init();
  expander->begin();

  /* Set expander pin 3 as output to enable the amplifier */
  expander->pinMode(3, OUTPUT);
  expander->digitalWrite(3, HIGH); /* Enable amplifier */
}

void loop() {
  /* Toggle sign every half wavelength to create a square wave */
  if (count % (48000/(2*frequency)) == 0) {
    sample = -sample;
  }

  /* Write the sample (mono) */
  i2s.write(sample); // Left channel

  /* Increment sample counter */
  count++;
}
