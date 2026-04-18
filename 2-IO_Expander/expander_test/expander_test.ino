/**
 * Reads and displays the status of signals connected to a TCA95XX_16BIT I/O expander via I2C.
 * Configures pins as inputs or outputs and updates the reading every second.
 * Shows each pin's name and state on the serial monitor.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <esp_io_expander.hpp> /* Library to control I/O expanders on ESP32 */

#define I2C_SCL_PIN     (47)   /* I2C bus SCL pin */
#define I2C_SDA_PIN     (48)   /* I2C bus SDA pin */
#define I2C_ADDR        (0x20) /* I2C address of the expander */

esp_expander::Base *expander = nullptr; /* Pointer to expander object */

const char* signalNames[14] = {          /* Descriptive signal names */
  "Magnetometer - INT",        // EXP0
  "RTC - INTB",                // EXP1
  "RTC - INTA",                // EXP2
  "Battery Indicator - GPOUT", // EXP5
  "Pad - Up",                  // EXP6
  "Pad - Left",                // EXP7
  "Pad - Down",                // EXP8
  "Button - Down",             // EXP9
  "PMIC - INT",                // EXP10
  "Pad - Right",               // EXP11
  "IMU - INT2",                // EXP12
  "IMU - INT1",                // EXP13
  "microSD - CD",              // EXP14
  "Display - TP INT"           // EXP15
};

void setup() {
  Serial.begin(115200); /* Starts serial communication at 115200 baud */
  Serial.println("Expander polling test start.");

  /* Initializes the expander at the configured I2C address */
  expander = new esp_expander::TCA95XX_16BIT(I2C_SCL_PIN, I2C_SDA_PIN, I2C_ADDR);
  expander->init();
  expander->begin();

  /* Sets pins 3 and 4 as outputs */
  expander->multiPinMode(IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4, OUTPUT);

  /* Sets all other pins as inputs */
  expander->multiPinMode(IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
                         IO_EXPANDER_PIN_NUM_5 | IO_EXPANDER_PIN_NUM_6 | IO_EXPANDER_PIN_NUM_7 |
                         IO_EXPANDER_PIN_NUM_8 | IO_EXPANDER_PIN_NUM_9 | IO_EXPANDER_PIN_NUM_10 |
                         IO_EXPANDER_PIN_NUM_11 | IO_EXPANDER_PIN_NUM_12 | IO_EXPANDER_PIN_NUM_13 |
                         IO_EXPANDER_PIN_NUM_14 | IO_EXPANDER_PIN_NUM_15, INPUT);

  /* Sets controlled outputs to LOW */
  expander->digitalWrite(IO_EXPANDER_PIN_NUM_3, LOW); /* Amplifier - SD */
  expander->digitalWrite(IO_EXPANDER_PIN_NUM_4, LOW); /* Power Supply - 3V3 peripherals */
}

int level[14] = {0}; /* Input states */

void loop() {
  /* Reads the state of each input pin on the expander */
  level[0] = expander->digitalRead(0);
  level[1] = expander->digitalRead(1);
  level[2] = expander->digitalRead(2);
  level[3] = expander->digitalRead(5);
  level[4] = expander->digitalRead(6);
  level[5] = expander->digitalRead(7);
  level[6] = expander->digitalRead(8);
  level[7] = expander->digitalRead(9);
  level[8] = expander->digitalRead(10);
  level[9] = expander->digitalRead(11);
  level[10] = expander->digitalRead(12);
  level[11] = expander->digitalRead(13);
  level[12] = expander->digitalRead(14);
  level[13] = expander->digitalRead(15);

  /* Displays the status of each signal on the serial monitor */
  Serial.println("=== Pin Status ===");
  for (int i = 0; i < 14; i++) {
    Serial.printf("EXP%02d: %d - %s\n", i, level[i], signalNames[i]);
  }
  Serial.println();

  delay(1000); /* Waits 1 second before the next read */
}
