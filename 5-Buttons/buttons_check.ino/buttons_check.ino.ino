/**
 * Detects button presses from a TCA95XX_16BIT I/O expander and a direct ESP32-S3 pin.
 * Uses interrupts to respond to button events without constant polling.
 * Prints the detected button to the serial monitor.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <esp_io_expander.hpp> /* Library to control I/O expanders on ESP32 */

/* Expander configuration */
#define CHIP_NAME         TCA95XX_16BIT
#define I2C_SCL_PIN       (47)  /* I2C bus SCL pin */
#define I2C_SDA_PIN       (48)  /* I2C bus SDA pin */
#define EXP_INT_PIN       (18)  /* Expander interrupt pin */
#define I2C_ADDR          (0x20)/* Expander I2C address */

/* Button connections on expander */
#define PAD_UP            6
#define PAD_LEFT          7
#define PAD_DOWN          8
#define PAD_RIGHT         11
#define BUTTON_BOTTOM     9

/* Button connected directly to ESP32-S3 */
#define BUTTON_UP_PIN     0    /* GPIO0 */

/* Expander instance */
esp_expander::Base *expander = nullptr;

/* Flags for pending interrupts */
volatile bool expanderInterrupted = false;
volatile bool buttonUpInterrupted = false;

/* ISR for expander interrupt */
void IRAM_ATTR handleExpanderIRQ() {
  expanderInterrupted = true;
}

/* ISR for button on GPIO0 */
void IRAM_ATTR handleButtonUpIRQ() {
  buttonUpInterrupted = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Button interrupt test start");

  /* Initialize expander */
  expander = new esp_expander::TCA95XX_16BIT(I2C_SCL_PIN, I2C_SDA_PIN, I2C_ADDR);
  expander->init();
  expander->begin();

  /* Configure expander pins as inputs */
  expander->pinMode(PAD_UP,        INPUT);
  expander->pinMode(PAD_LEFT,      INPUT);
  expander->pinMode(PAD_DOWN,      INPUT);
  expander->pinMode(PAD_RIGHT,     INPUT);
  expander->pinMode(BUTTON_BOTTOM, INPUT);

  /* Configure expander interrupt pin */
  pinMode(EXP_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(EXP_INT_PIN),
                  handleExpanderIRQ, FALLING);

  /* Configure direct button on GPIO0 */
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_UP_PIN),
                  handleButtonUpIRQ, FALLING);

  Serial.println("Setup complete. Waiting for button presses...");
}

void loop() {
  /* If there are no pending interrupts, wait */
  if (!expanderInterrupted && !buttonUpInterrupted) {
    delay(10);
    return;
  }

  /* Handle direct button */
  if (buttonUpInterrupted) {
    buttonUpInterrupted = false;
    Serial.println("→ BUTTON_UP (GPIO0) pressed");
    delay(50);
  }

  /* Handle expander buttons */
  if (expanderInterrupted) {
    expanderInterrupted = false;
    if (expander->digitalRead(PAD_UP) == LOW) {
      Serial.println("→ PAD_UP pressed");
    }
    if (expander->digitalRead(PAD_LEFT) == LOW) {
      Serial.println("→ PAD_LEFT pressed");
    }
    if (expander->digitalRead(PAD_DOWN) == LOW) {
      Serial.println("→ PAD_DOWN pressed");
    }
    if (expander->digitalRead(BUTTON_BOTTOM) == LOW) {
      Serial.println("→ BUTTON_BOTTOM pressed");
    }
    if (expander->digitalRead(PAD_RIGHT) == LOW) {
      Serial.println("→ PAD_RIGHT pressed");
    }
    delay(50);
  }
}
