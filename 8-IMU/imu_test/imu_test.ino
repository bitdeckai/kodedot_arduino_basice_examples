/**
 * Initializes and reads an LSM6DSOX IMU over I2C on ESP32-S3, printing accel, gyro, and temperature.
 * Prints configured ranges and data rates, then outputs readings every 1 second over Serial.
 * Uses custom I2C pins (GPIO48/47) and the Adafruit_LSM6DSOX library.
*/
/* ───────── KODE | docs.kode.diy ───────── */

#include <Adafruit_LSM6DSOX.h>  /* Library for the LSM6DSOX sensor */
#include <Wire.h>               /* I2C communication library */

/* Configurable I2C pins */
#define I2C_SDA 48            /* SDA pin */
#define I2C_SCL 47            /* SCL pin */

/* IMU sensor instance and I2C bus object */
Adafruit_LSM6DSOX imu;

void setup(void) {
  /* Initialize serial port for debugging */
  Serial.begin(115200);
  while (!Serial);

  /* Initialize the I2C bus with the specified pins */
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("LSM6DSOX test");

  /* Attempt to initialize the sensor at the default I2C address (0x6A).
     Note: some boards use 0x6B depending on SA0. */
  if (!imu.begin_I2C()) {
    Serial.println("Failed to find LSM6DSOX chip");
    while (1) {
      delay(10);  /* Infinite loop if initialization fails */
    }
  }
  Serial.println("LSM6DSOX Found!");

  /* Display configured accelerometer range */
  Serial.print("Accelerometer range set to: ");
  switch (imu.getAccelRange()) {
    case LSM6DS_ACCEL_RANGE_2_G:
      Serial.println("+-2G"); break;
    case LSM6DS_ACCEL_RANGE_4_G:
      Serial.println("+-4G"); break;
    case LSM6DS_ACCEL_RANGE_8_G:
      Serial.println("+-8G"); break;
    case LSM6DS_ACCEL_RANGE_16_G:
      Serial.println("+-16G"); break;
  }

  /* Display configured gyroscope range */
  Serial.print("Gyro range set to: ");
  switch (imu.getGyroRange()) {
    case LSM6DS_GYRO_RANGE_125_DPS:
      Serial.println("125 degrees/s"); break;
    case LSM6DS_GYRO_RANGE_250_DPS:
      Serial.println("250 degrees/s"); break;
    case LSM6DS_GYRO_RANGE_500_DPS:
      Serial.println("500 degrees/s"); break;
    case LSM6DS_GYRO_RANGE_1000_DPS:
      Serial.println("1000 degrees/s"); break;
    case LSM6DS_GYRO_RANGE_2000_DPS:
      Serial.println("2000 degrees/s"); break;
    case ISM330DHCX_GYRO_RANGE_4000_DPS:
      /* Unsupported range for the DSOX */
      break;
  }

  /* Display accelerometer data rate */
  Serial.print("Accelerometer data rate set to: ");
  switch (imu.getAccelDataRate()) {
    case LSM6DS_RATE_SHUTDOWN:    Serial.println("0 Hz"); break;
    case LSM6DS_RATE_12_5_HZ:     Serial.println("12.5 Hz"); break;
    case LSM6DS_RATE_26_HZ:       Serial.println("26 Hz"); break;
    case LSM6DS_RATE_52_HZ:       Serial.println("52 Hz"); break;
    case LSM6DS_RATE_104_HZ:      Serial.println("104 Hz"); break;
    case LSM6DS_RATE_208_HZ:      Serial.println("208 Hz"); break;
    case LSM6DS_RATE_416_HZ:      Serial.println("416 Hz"); break;
    case LSM6DS_RATE_833_HZ:      Serial.println("833 Hz"); break;
    case LSM6DS_RATE_1_66K_HZ:    Serial.println("1.66 KHz"); break;
    case LSM6DS_RATE_3_33K_HZ:    Serial.println("3.33 KHz"); break;
    case LSM6DS_RATE_6_66K_HZ:    Serial.println("6.66 KHz"); break;
  }

  /* Display gyroscope data rate */
  Serial.print("Gyro data rate set to: ");
  switch (imu.getGyroDataRate()) {
    case LSM6DS_RATE_SHUTDOWN:    Serial.println("0 Hz"); break;
    case LSM6DS_RATE_12_5_HZ:     Serial.println("12.5 Hz"); break;
    case LSM6DS_RATE_26_HZ:       Serial.println("26 Hz"); break;
    case LSM6DS_RATE_52_HZ:       Serial.println("52 Hz"); break;
    case LSM6DS_RATE_104_HZ:      Serial.println("104 Hz"); break;
    case LSM6DS_RATE_208_HZ:      Serial.println("208 Hz"); break;
    case LSM6DS_RATE_416_HZ:      Serial.println("416 Hz"); break;
    case LSM6DS_RATE_833_HZ:      Serial.println("833 Hz"); break;
    case LSM6DS_RATE_1_66K_HZ:    Serial.println("1.66 KHz"); break;
    case LSM6DS_RATE_3_33K_HZ:    Serial.println("3.33 KHz"); break;
    case LSM6DS_RATE_6_66K_HZ:    Serial.println("6.66 KHz"); break;
  }
}

void loop() {
  /* Variables to hold sensor events */
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  /* Get accelerometer, gyroscope, and temperature events */
  imu.getEvent(&accel, &gyro, &temp);

  /* Print temperature in degrees Celsius */
  Serial.print("\t\tTemperature: ");
  Serial.print(temp.temperature);
  Serial.println(" deg C");

  /* Print acceleration in m/s^2 for each axis */
  Serial.print("\t\tAccel X: ");
  Serial.print(accel.acceleration.x);
  Serial.print(" \tY: ");
  Serial.print(accel.acceleration.y);
  Serial.print(" \tZ: ");
  Serial.print(accel.acceleration.z);
  Serial.println(" m/s^2");

  /* Print gyroscope rotation in rad/s for each axis */
  Serial.print("\t\tGyro X: ");
  Serial.print(gyro.gyro.x);
  Serial.print(" \tY: ");
  Serial.print(gyro.gyro.y);
  Serial.print(" \tZ: ");
  Serial.print(gyro.gyro.z);
  Serial.println(" radians/s");

  Serial.println();
  delay(1000);  /* Small delay between readings */
}
