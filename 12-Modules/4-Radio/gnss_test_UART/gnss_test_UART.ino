#include <SparkFun_u-blox_GNSS_v3.h>

/* GNSS object for serial communication */
SFE_UBLOX_GNSS_SERIAL myGNSS;

/* ─── GNSS UART pin mapping ───
 * GNSS_RX_PIN → MCU pin that receives data from GNSS TX
 * GNSS_TX_PIN → MCU pin that sends data to GNSS RX
 */
static const int GNSS_RX_PIN = 44; // MCU receives on GPIO44
static const int GNSS_TX_PIN = 43; // MCU transmits on GPIO43

/* Hardware serial instance (UART1) */
HardwareSerial GNSSSerial(1);

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("u-blox GNSS via UART1 (GPIO44/43)");

  /* ─── Initialize UART1 on the specified pins ───
   * Baud rate: 38400
   * Format: 8 data bits, no parity, 1 stop bit (SERIAL_8N1)
   */
  GNSSSerial.begin(38400, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  /* Enable GNSS debug messages on main Serial (optional) */
  myGNSS.enableDebugging(Serial);

  /* Configure UART1 output → UBX protocol only (disable NMEA messages) */
  myGNSS.setUART1Output(COM_TYPE_UBX);

  /* Attempt to connect to the GNSS module until successful */
  while (!myGNSS.begin(GNSSSerial)) {
    Serial.println(F("u-blox GNSS not detected. Retrying..."));
    delay(1000);
  }
}

void loop()
{
  /* If PVT (Position, Velocity, Time) data is available, read and display it */
  if (myGNSS.getPVT()) {
    int32_t lat = myGNSS.getLatitude();      // Latitude in degrees * 10^-7
    int32_t lon = myGNSS.getLongitude();     // Longitude in degrees * 10^-7
    int32_t alt = myGNSS.getAltitudeMSL();   // Altitude MSL in millimeters

    Serial.print(F("Lat: ")); Serial.print(lat);
    Serial.print(F(" Long: ")); Serial.print(lon);
    Serial.print(F(" (deg*1e-7) Alt: ")); Serial.print(alt);
    Serial.println(F(" (mm)"));
  }
}
