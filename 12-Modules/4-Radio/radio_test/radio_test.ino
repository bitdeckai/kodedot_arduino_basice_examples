/**
 * LR1121 Channel Scanner (RX only, no TX).
 * Scans 868 MHz and 2.4 GHz bands with dynamic BW and SF changes.
 * Measures RSSI (channel noise) and prints results in CSV format.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <Arduino.h>
#include <RadioLib.h>

/* Pin configuration (based on your design) */
#define NSS_PIN    3    /* Chip Select */
#define DIO1_PIN   12   /* IRQ */
#define NRST_PIN   2    /* Reset */
#define BUSY_PIN   13   /* Busy */
#define MISO_PIN   41
#define MOSI_PIN   40
#define SCK_PIN    39

/* RF Switch mapping for E80-900M2213S */
static const uint32_t rfswitch_dio_pins[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};
static const Module::RfSwitchMode_t rfswitch_table[] = {
  { LR11x0::MODE_STBY,  { LOW,  LOW  } },
  { LR11x0::MODE_RX,    { LOW,  LOW  } },  /* RX */
  { LR11x0::MODE_TX,    { LOW,  HIGH } },  /* TX Sub-1GHz LP */
  { LR11x0::MODE_TX_HP, { HIGH, LOW  } },  /* TX Sub-1GHz HP */
  { LR11x0::MODE_TX_HF, { HIGH, HIGH } },  /* TX 2.4GHz */
  { LR11x0::MODE_GNSS,  { LOW,  LOW  } },
  { LR11x0::MODE_WIFI,  { LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

/* Instances */
SPIClass spi(HSPI);
LR1121 radio = new Module(NSS_PIN, DIO1_PIN, NRST_PIN, BUSY_PIN, spi);

/* Frequency sweep settings */
const float FREQS_868[] = { 863.0, 866.0, 868.0, 869.5 };
const float FREQS_24[]  = { 2403.5, 2425.0, 2450.0, 2479.5 };
const size_t N_868 = sizeof(FREQS_868) / sizeof(FREQS_868[0]);
const size_t N_24  = sizeof(FREQS_24)  / sizeof(FREQS_24[0]);

/* Dynamic parameters */
const float  BWS_KHZ[] = { 125.0, 203.125 };
const int    SFS[]     = { 7, 9, 12 };
const int    CR        = 5;
const int    PWR_DBM   = 10;
const uint8_t PREAMBLE = 8;
const float  TCXO_V    = 1.8;

/* Listening time per measurement point */
const uint16_t DWELL_MS = 200;

/* Utilities */
/* Hardware reset for LR1121 */
static void hardResetModule() {
  pinMode(NRST_PIN, OUTPUT);
  digitalWrite(NRST_PIN, LOW);
  delay(50);
  digitalWrite(NRST_PIN, HIGH);
  delay(50);
}

/* Configure LR1121 with given parameters */
static bool configRadio(float freqMHz, float bwkHz, int sf) {
  int st = radio.begin(freqMHz, bwkHz, sf, CR, 0x12 /*sync*/, PWR_DBM, PREAMBLE, TCXO_V);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("Config FAIL  f="); Serial.print(freqMHz, 3);
    Serial.print(" MHz  BW="); Serial.print(bwkHz, 3);
    Serial.print(" kHz  SF="); Serial.print(sf);
    Serial.print("  code="); Serial.println(st);
    return false;
  }
  return true;
}

/* Measure RSSI once at given parameters and print CSV line */
static void measureOnce(float freqMHz, float bwkHz, int sf, const char* bandTag) {
  if (!configRadio(freqMHz, bwkHz, sf)) return;

  /* Start RX */
  int st = radio.startReceive();
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("RX start FAIL code="); Serial.println(st);
    return;
  }

  delay(DWELL_MS);
  float rssi = radio.getRSSI();

  /* CSV format output: BAND,FREQ_MHz,BW_kHz,SF,RSSI_dBm */
  Serial.print(bandTag); Serial.print(",");
  Serial.print(freqMHz, 3); Serial.print(",");
  Serial.print(bwkHz, 3); Serial.print(",");
  Serial.print(sf); Serial.print(",");
  Serial.println(rssi, 1);

  radio.standby();
}

/* Scan a full band across frequencies, BWs and SFs */
static void scanBand(const float* freqs, size_t nFreq, const char* bandTag) {
  Serial.println();
  Serial.println("BAND,FREQ_MHz,BW_kHz,SF,RSSI_dBm");
  for (size_t i = 0; i < nFreq; i++) {
    for (size_t b = 0; b < sizeof(BWS_KHZ)/sizeof(BWS_KHZ[0]); b++) {
      for (size_t s = 0; s < sizeof(SFS)/sizeof(SFS[0]); s++) {
        measureOnce(freqs[i], BWS_KHZ[b], SFS[s], bandTag);
      }
    }
  }
}

/* Setup */
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("\n=== LR1121 Channel Scanner (RSSI only) ===");
  Serial.println("RSSI = channel noise (closer to 0 => more noise)");

  pinMode(BUSY_PIN, INPUT);

  Serial.println("Resetting module...");
  hardResetModule();

  Serial.println("Initializing SPI...");
  spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN, NSS_PIN);

  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

  Serial.println("\n--- Scanning: 868 MHz ---");
  scanBand(FREQS_868, N_868, "868MHz");

  Serial.println("\n--- Scanning: 2.4 GHz ---");
  scanBand(FREQS_24, N_24, "2400MHz");

  Serial.println("\nScan complete. Restart to repeat.");
}

/* Loop */
void loop() {
  delay(1000);
}
