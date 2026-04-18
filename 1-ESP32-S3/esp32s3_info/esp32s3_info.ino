/**
 * Displays ESP32-S3 microcontroller information via serial port.
 * Includes model, revision, number of cores, and Chip ID.
 * Prints the data every 3 seconds.
 */
/* ───────── KODE | docs.kode.diy ───────── */

void setup() {
  Serial.begin(115200); /* Starts serial communication at 115200 baud */
}

void loop() {
  /* Prints ESP32 chip model and revision */
  Serial.printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  
  /* Prints the number of cores in the chip */
  Serial.printf("This chip has %d cores\n", ESP.getChipCores());
  
  /* Prints the chip's unique identifier */
  Serial.print("Chip ID: "); 
  Serial.println(ESP.getEfuseMac()); 

  /* Waits 3 seconds before repeating */
  delay(3000); 
}
