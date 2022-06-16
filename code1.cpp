#ifdef ENABLE_DEBUG
#define DEBUG_ESP_PORT Serial
#define NODEBUG_WEBSOCKETS
#define NDEBUG
#endif
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include "productConfig.h"
#define CONFIG_FILE       "/config2.json"
#define BAUDRATE          115200
#define DEVICE_RESET_PIN  0
#define relay1 D0
#define relay2 D5
#define Button1 D6
#define Button2 D7

bool rele1;
bool rele2;

DeviceConfig config;

bool powerState = false;

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("[onPowerState]: Device %s turned %s (via SinricPro) \r\n", deviceId.c_str(), state ? "on" : "off");
  powerState = state;
  if(deviceId==config.sw1_id)
  {
    
   if(state==HIGH)
   {
    digitalWrite(relay1,HIGH); 
   }
   else
   {
    digitalWrite(relay1,LOW);
   }
  }
  if(deviceId==config.sw2_id)
  {
   if(state==HIGH)
   {
    digitalWrite(relay2,HIGH); 
   }
   else
   {
    digitalWrite(relay2,LOW);
   }
  } 
  return true; // request handled properly
}

void handleFullReset() {
  if (!digitalRead(DEVICE_RESET_PIN))
  {
    Serial.printf("FULL RESET!\r\n");
    SPIFFS.begin();
    Serial.printf("DELETING CONFIG FILE!\r\n");
    SPIFFS.remove(CONFIG_FILE);
    delay(1000);
    WiFi.disconnect(true);
    delay(1000);
    SPIFFS.end();
    delay(1000);
    Serial.printf("REBOOT\r\n");
    ESP.restart();
  }
}
  void setupSinricPro() {
  SinricProSwitch &sw1 = SinricPro[config.sw1_id];
  sw1.onPowerState(onPowerState);
  SinricProSwitch &sw2 = SinricPro[config.sw2_id];
  sw2.onPowerState(onPowerState);
  SinricPro.onConnected([]() { Serial.printf("[SinricPro]: Connected to SinricPro\r\n"); });

  
  SinricPro.onDisconnected([]() {Serial.printf("[SinricPro]: Disconnected to SinricPro\r\n"); });
  SinricPro.begin(config.appKey, config.appSecret);
  }
  
void setup() {
  Serial.begin(BAUDRATE); 
  Serial.println();
  pinMode(relay1,OUTPUT);
  pinMode(relay2,OUTPUT);
  pinMode(Button1,INPUT);
  pinMode(Button2, INPUT);
  digitalWrite(relay1,LOW);
  digitalWrite(relay2,LOW);
  ConfigServer configServer(CONFIG_FILE, config);
  Serial.printf("Setup SinricPro!\r\n");
  setupSinricPro();
  pinMode(DEVICE_RESET_PIN, INPUT_PULLUP);
}
void loop() {
  handleFullReset();
  SinricPro.handle();
 if(digitalRead(Button1) == HIGH)
 {
 rele1=!rele1 ;
 digitalWrite(relay1,rele1);
 }
 if(digitalRead(Button2) == HIGH)
 {
 rele2=!rele2 ;
 digitalWrite(relay2,rele2);
 }
 delay(300);
}