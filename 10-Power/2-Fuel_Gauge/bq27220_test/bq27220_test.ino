/**
 * Example usage of the Texas Instruments BQ27220 battery fuel gauge.
 * Reads state of charge, voltage, current, and temperature over I²C.
 * Displays charging status and estimated time to full when charging.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <Wire.h>
#include <BQ27220.h>

/* I²C pin configuration for ESP32-S3 */
#define SDA_PIN 48   /* SDA line */
#define SCL_PIN 47   /* SCL line */

/* Battery fuel gauge driver instance */
BQ27220 gauge;

void setup() {
  /* Initialize serial port for debug output */
  Serial.begin(115200);

  /* Start I²C bus with custom SDA/SCL pins */
  Wire.begin(SDA_PIN, SCL_PIN);

  /* Initialize the BQ27220 fuel gauge */
  if (!gauge.begin()) {
    Serial.println("BQ27220 not found!");
    while (1) delay(1000); /* Halt execution if not found */
  }

  Serial.println("BQ27220 ready.");
}

void loop() {
  /* Read battery parameters */
  int soc = gauge.readStateOfChargePercent();  // State of charge (%)
  int mv  = gauge.readVoltageMillivolts();     // Voltage (mV)
  int ma  = gauge.readCurrentMilliamps();      // Current (mA), positive = charging
  float tC = gauge.readTemperatureCelsius();  // Temperature (°C)

  /* Print basic battery information */
  Serial.print("SOC= "); Serial.print(soc); Serial.print("%  ");
  Serial.print("V= "); Serial.print(mv); Serial.print(" mV  ");
  Serial.print("I= "); Serial.print(ma); Serial.print(" mA  ");
  Serial.print("T= "); Serial.print(tC, 1); Serial.print(" °C");

  /* If charging, show estimated time to full */
  if (ma > 0) {
    int ttf = gauge.readTimeToFullMinutes();
    Serial.print("  TTF= "); Serial.print(ttf); Serial.print(" min");
  }

  Serial.println();

  delay(1000); /* Update once per second */
}
