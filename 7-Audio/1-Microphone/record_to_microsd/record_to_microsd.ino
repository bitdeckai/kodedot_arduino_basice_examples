/**
 * Records 5 seconds of audio via I2S (48 kHz, 32-bit, mono) and saves it to /sdcard/test.wav.
 * Uses the ESP32-S3 I2S bus and a microSD in SD_MMC 1-bit mode with custom pins.
 * Prints initialization, recording, and write status over Serial.
*/
/* ───────── KODE | docs.kode.diy ───────── */

#include "ESP_I2S.h"
#include "FS.h"
#include "SD_MMC.h"

/* I2S pin assignments */
const uint8_t I2S_SCK   = 38;  /* Serial clock pin (SCK) */
const uint8_t I2S_WS    = 45;  /* Word select pin (LRCLK) */
const uint8_t I2S_DIN   = 21;  /* Data input pin (SD) */

/* SD card pin assignments for SD_MMC in 1-bit mode */
const uint8_t SD_CMD    = 5;   /* Command pin (CMD) */
const uint8_t SD_CLK    = 6;   /* Clock pin (CLK) */
const uint8_t SD_DATA0  = 7;   /* Data0 pin (D0) */

/* Create I2S interface instance */
I2SClass i2s;

/* Variables to store WAV data and its size */
uint8_t *wav_buffer;
size_t wav_size;

void setup() {
  /* Initialize serial port for debugging */
  Serial.begin(115200);
  Serial.println("Starting setup...");

  /* Configure I2S pins (MCLK not used: pass -1) */
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);

  /* Initialize I2S in standard mode: 48 kHz, 32-bit data, mono, left-aligned slot */
  Serial.println("Initializing I2S bus...");
  if (!i2s.begin(
        I2S_MODE_STD,
        48000,
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
  }
  Serial.println("I2S bus initialized.");

  /* Configure SD_MMC pins for 1-bit SD mode */
  Serial.println("Configuring SD card pins...");
  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_DATA0)) {
    Serial.println("Failed to configure SD pins!");
    return;
  }

  /* Mount the SD card at "/sdcard" */
  Serial.println("Mounting SD card...");
  if (!SD_MMC.begin("/sdcard", true)) {  /* true => 1-bit bus */
    Serial.println("Failed to initialize SD card!");
    return;
  }
  Serial.println("SD card mounted successfully.");

  /* Notify that recording will start */
  Serial.println("Recording 5 seconds of audio...");

  /* Record WAV data for 5 seconds into wav_buffer */
  wav_buffer = i2s.recordWAV(5, &wav_size);
  Serial.printf("Recorded size: %d bytes\n", wav_size);

  /* Open a file on the SD card for writing */
  File file = SD_MMC.open("/test.wav", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing!");
    return;
  }

  /* Write recorded data and verify complete write */
  Serial.println("Writing audio data to file...");
  if (file.write(wav_buffer, wav_size) != wav_size) {
    Serial.println("Failed to write audio data to file!");
    file.close();
    return;
  }

  /* Close the file when done */
  file.close();
  Serial.println("Audio recording and save complete.");
}

void loop() {
  delay(10000);
}
