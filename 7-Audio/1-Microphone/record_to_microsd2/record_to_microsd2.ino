#include "ESP_I2S.h"
#include "FS.h"
#include "SD_MMC.h"

/* I2S pins */
const uint8_t I2S_SCK   = 38;
const uint8_t I2S_WS    = 45;
const uint8_t I2S_DIN   = 21;   // Mic
const uint8_t I2S_DOUT  = 46;   // Speaker

/* SD_MMC pins (1-bit mode) */
const uint8_t SD_CMD    = 5;
const uint8_t SD_CLK    = 6;
const uint8_t SD_DATA0  = 7;

I2SClass i2s;

uint8_t *wav_buffer;
size_t wav_size;

void setup() {
  Serial.begin(115200);
  Serial.println("Start...");

  /* ------------ I2S RX (录音模式) ------------ */
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);

  if (!i2s.begin(
        I2S_MODE_STD,
        48000,
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_LEFT)) {
    Serial.println("I2S init failed!");
    return;
  }

  /* ------------ SD 初始化 ------------ */
  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_DATA0)) {
    Serial.println("SD pin config failed!");
    return;
  }

  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD mount failed!");
    return;
  }

  Serial.println("Recording 5 seconds...");
  wav_buffer = i2s.recordWAV(5, &wav_size);

  Serial.printf("Recorded %d bytes\n", wav_size);

  File file = SD_MMC.open("/test.wav", FILE_WRITE);
  file.write(wav_buffer, wav_size);
  file.close();

  Serial.println("Saved to SD.");

  delay(1000);

  /* ------------ 切换到播放模式 ------------ */
  Serial.println("Switching to playback...");

  i2s.end();  // 关闭 RX

  /* 设置 TX 引脚 */
  i2s.setPins(I2S_SCK, I2S_WS, I2S_DOUT, -1);

  if (!i2s.begin(
        I2S_MODE_STD,
        48000,
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_LEFT)) {
    Serial.println("I2S TX init failed!");
    return;
  }

  playWavFromSD("/test.wav");

  Serial.println("Playback finished.");
}

void loop() {
  delay(10000);
}

/* ------------ WAV 播放函数 ------------ */
void playWavFromSD(const char *path) {

  File file = SD_MMC.open(path);
  if (!file) {
    Serial.println("Failed to open WAV file!");
    return;
  }

  /* 跳过 WAV header (44 bytes) */
  file.seek(44);

  const size_t bufferSize = 1024;
  uint8_t buffer[bufferSize];

  while (file.available()) {
    size_t bytesRead = file.read(buffer, bufferSize);
    i2s.write(buffer, bytesRead);
  }

  file.close();
}
