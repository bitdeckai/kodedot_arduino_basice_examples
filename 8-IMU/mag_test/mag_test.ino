/**
 * Initializes and reads the LIS2MDL magnetometer over I2C on an ESP32-S3.
 * Prints sensor details on startup, then outputs the magnetic vector (uT) every second.
 * Uses custom I2C pins (GPIO48/47) and the Adafruit_LIS2MDL library.
*/
/* ───────── KODE | docs.kode.diy ───────── */

#include <Wire.h>
#include <Adafruit_LIS2MDL.h>

/* Configurable I2C pins */
#define I2C_SDA 48            /* SDA pin */
#define I2C_SCL 47            /* SCL pin */

/* Magnetometer instance with unique ID */
Adafruit_LIS2MDL mag = Adafruit_LIS2MDL(12345);

void setup(void) {
  /* Initialize serial port for debug output */
  Serial.begin(115200);
  while (!Serial) {
    /* wait for serial */
  }

  /* Initialize I2C bus with selected SDA, SCL pins */
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("Magnetometer Test");
  Serial.println();

  /* Attempt to initialize the LIS2MDL sensor at I2C address 0x1E */
  if (!mag.begin()) {
    /* Sensor not detected: print error and halt */
    Serial.println("Ooops, no LIS2MDL detected ... Check your wiring!");
    while (1) {
      delay(10);  /* Infinite loop on failure */
    }
  }

  /* Display some basic information on this sensor */
  mag.printSensorDetails();
}

void loop(void) {
  /* Get a new sensor event */
  sensors_event_t event;
  mag.getEvent(&event);

  /* Display the results (magnetic vector values are in micro-Tesla (uT)) */
  Serial.print("X: ");
  Serial.print(event.magnetic.x);
  Serial.print("  ");
  Serial.print("Y: ");
  Serial.print(event.magnetic.y);
  Serial.print("  ");
  Serial.print("Z: ");
  Serial.print(event.magnetic.z);
  Serial.print("  ");
  Serial.println("uT");

  /* Delay before next reading */
  delay(1000);
}
