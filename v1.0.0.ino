#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/*"esp32-hal-dac.h"****************************************/
#ifndef MAIN_ESP32_HAL_DAC_H_
#define MAIN_ESP32_HAL_DAC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
#include "driver/gpio.h"

void dacWrite(uint8_t pin, uint8_t value);
void dacDisable(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_DAC_H_ */
/**********************************************************/
/*
   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.
*/   

#define DEBUG
//#undef DEBUG

#define buttonINTRPin 16 //use pin16 button input as an interrupt
#define LED           17
#define motorPin      22 //DAC pin

#define SERVICE_UUID            "61b4b34a-e715-11ed-a05b-0242ac120003"
#define CHARACTERISTIC_UUID_RX  "61b4b69c-e715-11ed-a05b-0242ac120003"
#define CHARACTERISTIC_UUID_TX  "61b4b7fa-e715-11ed-a05b-0242ac120003"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;
bool LEDflag = false;
bool MOTORflag = false;
int BAT_LEVEL = 0;

String S_motor_STATE = F("Motor state: ");
String S_bat_LEVEL = F("Battery level: "); 

//BLE connection detection
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer){
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer){
    deviceConnected = false;
  }
};

class MyCallbacks: public BLECharacteristicCallbacks{
  void onWrite(BLECharacteristic *pCharacteristic){
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0){
      //test and show in serial monitor
      #ifdef DEBUG
        Serial.println("****************");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++){
          Serial.print(rxValue[i]);
        }
        Serial.println();
      #endif

      //motor on/off when receiving signal
      if (rxValue.find("MotorOn") != -1){
        #ifdef DEBUG
          Serial.print("Motor ON!");
        #endif
        dacWrite(motorPin, 255);
        MOTORflag = true;
      }
      else if (rxValue.find("MotorOff") != -1){
        #ifdef DEBUG
          Serial.print("Motor OFF!");
        #endif
        dacWrite(motorPin, 0);
        MOTORflag = false;
      }

      #ifdef DEBUG
        Serial.println();
        Serial.println("***************");
      #endif
    }
  }
};

void LEDblink(){
  while (!LEDflag){
    digitalWrite(LED, true);
    delay(1000);
  }
}

void IRAM_ATTR buttonISR_Callback(){
  //Create the BLE device
  BLEDevice::init("Mr.Ballby BLE UART");

  //Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  //Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  //Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_TX,
                       BLECharacteristic::PROPERTY_NOTIFY);

  /*According to Andreas Spiess' video, here the BLE2902 descriptor makes 
  it so that the ESP32 won't notify the client unless the client wants to 
  open its ears up to read the values to eliminate "talking to the air" and
  we also set the callback that handles receiving values via the RX channel.
  However, if you try uncommenting the BLE2902 line and even the BLE2902 
  #include line, the code still seems to run just as it did before! Maybe 
  someone more knowledgeable can tell us what's going on here! Next, we start 
  the BLE service and start advertising, but the ESP32 ain't gonna send 
  nothin' until a client connects!*/
  pCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                          CHARACTERISTIC_UUID_RX,
                                          BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCallbacks());
  
  //Start the service when the button is pushed
  pService->start();

  //Start Advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  //LED blink when waiting for connection
  LEDblink();
}

int BatteryLevelMeasurement(){
  //****************************************************to be continue...
  return BAT_LEVEL;
}

void setup() {
  #ifdef DEBUG
    Serial.begin(115200); //for debugging
  #endif
  
  pinMode(buttonINTRPin, INPUT);
  pinMode(LED,OUTPUT);
  pinMode(motorPin, OUTPUT);
  digitalWrite(LED, false);

  //Enable interrupt: when the state of push button change, start BLE
  attachInterrupt(digitalPinToInterrupt(buttonINTRPin), buttonISR_Callback, CHANGE);
}

void loop() {
  if (deviceConnected){
    //send motor state and battery level
    if (MOTORflag)
      S_motor_STATE = F("Motor state: ON");
    else
      S_motor_STATE = F("Motor state: OFF");
    pCharacteristic->setValue(S_motor_STATE.c_str());
    pCharacteristic->notify();
    #ifdef DEBUG
      Serial.print("*** Sent Value: ");
      Serial.print(S_motor_STATE);
      Serial.println(" ***");
    #endif

    delay(500);
    
    S_bat_LEVEL += String(BatteryLevelMeasurement());
    pCharacteristic->setValue(S_bat_LEVEL.c_str());
    pCharacteristic->notify();
    #ifdef DEBUG
      Serial.print("*** Sent Value: ");
      Serial.print(S_bat_LEVEL);
      Serial.println(" ***");
    #endif

    //pCharacteristic->setValue(&txValue, 1); // To send the integer value
    //pCharacteristic->setValue("Hello!"); // Sending a test message
  }
  delay(1000);
}
