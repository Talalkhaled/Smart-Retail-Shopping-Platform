#include "arduino_secrets.h"
// HX711-------
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
//-------------
#include "thingProperties.h"

int BALANCE = 20;

float previousSensorValue; // Store previous sensor value

//HX711--------
const int HX711_dout = 16; //mcu > HX711 dout pin
const int HX711_sck = 4; //mcu > HX711 sck pin
float initalWeight;
float weight;
int initalItemNum;
int itemNum;
int balanceSubtractFactor;
#define ITEM_WEIGHT 1497 // Average weight of sold item in the platform
#define ITEM_PRICE 5 // The price for one item
//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;
// --------------

void setup() {
  Serial.begin(9600);
  delay(1500);
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  Serial.println("Starting...");
  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  } else {
    LoadCell.setCalFactor(1.0);
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  calibrate();
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0;

  if (LoadCell.update()) newDataReady = true;

  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      initalWeight = LoadCell.getData();
      Serial.print("Initial load_cell output val: ");
      Serial.println(initalWeight);
      newDataReady = 0;
      t = millis();
    }
  }

  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
    else if (inByte == 'r') calibrate();
    else if (inByte == 'c') changeSavedCalFactor();
  }

  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

  previousSensorValue = initalWeight;
  itemNum = round(initalWeight / ITEM_WEIGHT);
  initalItemNum = itemNum;
  delay(500);

  while (1) {
    ArduinoCloud.update();
    while (newDataReady == false) {
      Serial.println("WHILE DATA READY!");
      if (LoadCell.update()) {
        newDataReady = true;
      }
    }


    if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
        weight = LoadCell.getData();
        Serial.print("Current load_cell output val: ");
        Serial.println(weight);
        newDataReady = 0;
        t = millis();
      }
    }

    Serial.print("Initial Weight> ");
    Serial.println(initalWeight);

    Serial.print("Current Weight> ");
    Serial.println(weight);

    Serial.print("\n#Number of items> ");
    itemNum = round(weight / ITEM_WEIGHT);
    Serial.println(itemNum);

    Serial.print("\nSESSION> ");
    Serial.println(session);

    Serial.print("\nBALANCE> ");
    Serial.println(BALANCE);

    if (session) {
      while (session) {
        ArduinoCloud.update();
        while (newDataReady == false) {
          if (LoadCell.update()) {
            newDataReady = true;
          }
        }

        if (newDataReady) {
          if (millis() > t + serialPrintInterval) {
            previousSensorValue = LoadCell.getData();
            Serial.print("Initial Weight After session> ");
            Serial.println(previousSensorValue);
            newDataReady = 0;
            t = millis();
          }
        }

        initalItemNum = round(previousSensorValue / ITEM_WEIGHT);
        Serial.print("Session itemNum> ");
        Serial.println(initalItemNum);
        balanceSubtractFactor = itemNum - initalItemNum;
      //  Serial.print("balanceSubtractFactor> ");
      //  Serial.println(balanceSubtractFactor);
        delay(1000);
      }

      initalWeight = previousSensorValue;


      // Update BALANCE once when session ends
      Serial.println(initalItemNum);
      Serial.println(itemNum);
      if (initalItemNum<itemNum){
        BALANCE -= balanceSubtractFactor * ITEM_PRICE; // If no items were misplaced this, balanceSubtractFactor will take care of zeroing the subtraction
      }

    }
      while (itemNum!=initalItemNum) {
        Serial.println("\n\n\n\n\n\n\n\n\n\nPlease start the session before placing or displacing items! ≖‿≖");
        itemNum = round(previousSensorValue / ITEM_WEIGHT);
        delay(2000);
      }
    delay(500);
  }
}

void onSessionChange() {
  // No need to do anything here if balance update is handled in loop()
}

void calibrate() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell on a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");
  Serial.println("Send 't' from serial monitor to set the tare offset.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      _resume = true;
    }
  }

  Serial.println("Now, place your known mass on the loadcell.");
  Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();
      if (known_mass != 0) {
        Serial.print("Known mass is: ");
        Serial.println(known_mass);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet();
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

  Serial.print("New calibration value has been set to: ");
  Serial.print(newCalibrationValue);
  Serial.println(", use this as calibration value (calFactor) in your project sketch.");
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  Serial.println("End calibration");
  Serial.println("***");
  Serial.println("To re-calibrate, send 'r' from serial monitor.");
  Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
}

void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  Serial.println("***");
  Serial.print("Current value is: ");
  Serial.println(oldCalibrationValue);
  Serial.println("Now, send the new value from serial monitor, i.e. 696.0");
  float newCalibrationValue;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print("New calibration value is: ");
        Serial.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  _resume = false;
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }
  Serial.println("End change calibration value");
  Serial.println("***");
}