/**
 * Example usage of the PMIC BQ25896 on ESP32-S3 via I²C.
 * Initializes the battery charger/power manager, displays system parameters,
 * and updates readings and status every second.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include "PMIC_BQ25896.h"
#include <Wire.h>

/* I²C bus configuration: SDA, SCL pins */
#define I2C_SDA 48           /* SDA pin */
#define I2C_SCL 47           /* SCL pin */

/* BQ25896 driver instance */
PMIC_BQ25896 bq25896;

void setup() {
  /* Initialize serial port for debugging */
  Serial.begin(115200);
  while (!Serial) {
    /* Wait for serial connection */
  }

  /* Initialize I²C bus with specified pins */
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("BQ25896 Power Management and Battery Charger Example");

  /* Initialize the BQ25896 over I²C */
  bq25896.begin();
  delay(500);  /* Allow device to power up */

  /* Check device connectivity */
  if (!bq25896.isConnected()) {
    Serial.println("BQ25896 not found! Check connection and power");
    while (1) {
      /* Halt execution if device is not found */
    }
  } else {
    Serial.println("BQ25896 found successfully.");
  }
}

void loop() {
  Serial.println("BQ25896 System Parameters");

  /* Input current limit pin status */
  Serial.print("ILIM PIN : ");
  Serial.println(String(bq25896.getILIM_reg().en_ilim));

  /* System and charging parameters */
  Serial.print("IINLIM       : "); Serial.println(String(bq25896.getIINLIM()) + " mA");
  Serial.print("VINDPM_OS    : "); Serial.println(String(bq25896.getVINDPM_OS()) + " mV");
  Serial.print("SYS_MIN      : "); Serial.println(String(bq25896.getSYS_MIN()) + " mV");
  Serial.print("ICHG         : "); Serial.println(String(bq25896.getICHG()) + " mA");
  Serial.print("IPRE         : "); Serial.println(String(bq25896.getIPRECHG()) + " mA");
  Serial.print("ITERM        : "); Serial.println(String(bq25896.getITERM()) + " mA");
  Serial.print("VREG         : "); Serial.println(String(bq25896.getVREG()) + " mV");
  Serial.print("BAT_COMP     : "); Serial.println(String(bq25896.getBAT_COMP()) + " mΩ");
  Serial.print("VCLAMP       : "); Serial.println(String(bq25896.getVCLAMP()) + " mV");
  Serial.print("BOOSTV       : "); Serial.println(String(bq25896.getBOOSTV()) + " mV");
  Serial.print("BOOST_LIM    : "); Serial.println(String(bq25896.getBOOST_LIM()) + " mA");
  Serial.print("VINDPM       : "); Serial.println(String(bq25896.getVINDPM()) + " mV");
  Serial.print("BATV         : "); Serial.println(String(bq25896.getBATV()) + " mV");
  Serial.print("SYSV         : "); Serial.println(String(bq25896.getSYSV()) + " mV");
  Serial.print("TSPCT        : "); Serial.println(String(bq25896.getTSPCT()) + "%");
  Serial.print("VBUSV        : "); Serial.println(String(bq25896.getVBUSV()) + " mV");
  Serial.print("ICHGR        : "); Serial.println(String(bq25896.getICHGR()) + " mA");

  /* Fault status */
  Serial.print("Fault -> "); 
  Serial.print("NTC:" + String(bq25896.getFAULT_reg().ntc_fault));
  Serial.print(" ,BAT:" + String(bq25896.getFAULT_reg().bat_fault));
  Serial.print(" ,CHGR:" + String(bq25896.getFAULT_reg().chrg_fault));
  Serial.print(" ,BOOST:" + String(bq25896.getFAULT_reg().boost_fault));
  Serial.println(" ,WATCHDOG:" + String(bq25896.getFAULT_reg().watchdog_fault));

  /* Charging status */
  Serial.print("Charging Status -> "); 
  Serial.print("CHG_EN:" + String(bq25896.getSYS_CTRL_reg().chg_config));
  Serial.print(" ,BATFET DIS:" + String(bq25896.getCTRL1_reg().batfet_dis));
  Serial.print(" ,BATLOAD_EN:" + String(bq25896.getSYS_CTRL_reg().bat_loaden));
  Serial.print(" ,PG STAT:" + String(bq25896.get_VBUS_STAT_reg().pg_stat));
  Serial.print(" ,VBUS STAT:" + String(bq25896.get_VBUS_STAT_reg().vbus_stat));
  Serial.print(" ,CHRG STAT:" + String(bq25896.get_VBUS_STAT_reg().chrg_stat));
  Serial.println(",VSYS STAT:" + String(bq25896.get_VBUS_STAT_reg().vsys_stat));

  /* Trigger a new ADC conversion */
  bq25896.setCONV_START(true);

  delay(1000); /* Update every second */
}
