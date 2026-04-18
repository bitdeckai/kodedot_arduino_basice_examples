/**
 * Example: Reading GNSS (GPS) data from a Kode Radio Module via I2C.
 * Retrieves latitude, longitude, and altitude from the u-blox module.
 * Uses the PVT (Position, Velocity, Time) message for data retrieval.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <SparkFun_u-blox_GNSS_v3.h>
#include <Wire.h>

/* GNSS module object */
SFE_UBLOX_GNSS myGNSS;

void setup()
{
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Kode Radio Module Example");

  /* ─── Initialize I2C with SDA = GPIO48, SCL = GPIO47 ─── */
  Wire.begin(48, 47);

  /* Enable GNSS debug messages on Serial (optional) */
  myGNSS.enableDebugging(); // Comment this line to disable debug messages

  /* Try to connect to the u-blox GNSS module until successful */
  while (myGNSS.begin() == false)
  {
    Serial.println(F("u-blox GNSS not detected. Retrying..."));
    delay(1000);
  }

  /* Set I2C output to UBX protocol only (disable NMEA messages) */
  myGNSS.setI2COutput(COM_TYPE_UBX);
}

void loop()
{
  /* If PVT data is available, read and display it */
  if (myGNSS.getPVT() == true)
  {
    /* Get and print latitude */
    int32_t latitude = myGNSS.getLatitude();
    Serial.print(F("Lat: "));
    Serial.print(latitude);

    /* Get and print longitude */
    int32_t longitude = myGNSS.getLongitude();
    Serial.print(F(" Long: "));
    Serial.print(longitude);
    Serial.print(F(" (degrees * 10^-7)"));

    /* Get and print altitude (MSL) */
    int32_t altitude = myGNSS.getAltitudeMSL();
    Serial.print(F(" Alt: "));
    Serial.print(altitude);
    Serial.print(F(" (mm)"));

    Serial.println();
  }
}
