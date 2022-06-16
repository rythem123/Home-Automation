#ifndef PRODUCTCONFIG_H
#define PRODUCTCONFIG_H
void touch();
bool rele3;
bool rele4;
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

#define PRODUCT_CODE "sinric.products.types.WALLY"
#define SWITCH_PRODUCT_CODE "sinric.devices.types.SWITCH"
#define FORMAT_SPIFFS_IF_FAILED true 

struct DeviceConfig
{
  char appKey[38];
  char appSecret[76];
  char sw1_id[26];
  char sw2_id[26];
};

class ConfigServer {
  public:
    ConfigServer(const String fileName, DeviceConfig &config);
    ~ConfigServer();

  private:
    void runConfigProcess();

    void startServer();
    void handleServer();
    void stopServer();

    void onConfigure();
    void onDeviceInfo();

    bool loadJsonConfig();
    bool saveJsonConfig(const JsonDocument &doc);

    void startSmartConfig();
    bool setupWiFi();

    void extractJsonConfig(const JsonDocument &doc);

    String getMacAddress();
    uint32_t getChipId32();
    
    void notifySuccess();
    bool reconnectToWiFi();

    String decodeStr(String input, String key);    
    void decryptBuf(char *buf, const char *input, const char *key, int input_len, int key_len);
     
    DeviceConfig &config;
    String fileName;    
    ESP8266WebServer *server;
    bool isConfigured;     
};

ConfigServer::ConfigServer(const String fileName, DeviceConfig &config) : config(config),
                                                                          fileName(fileName),
                                                                          server(nullptr)
{
  if(!SPIFFS.begin()){
      Serial.println("SPIFFS Mount Failed");
      return;
  }
 
  isConfigured = loadJsonConfig();
  if (!isConfigured) {
    startSmartConfig(); // wait until wifi is available
    startServer();      // setup server 
    handleServer();     // run server until config is received
    stopServer();       // stop server 
  } else {
    if (!setupWiFi()) {
      startSmartConfig(); // wait until wifi is available
      startServer();      // setup server
      handleServer();     // run server until config is received
      stopServer();       // stop server
    }
  }
}
void touch(){
 if(digitalRead(D6) == HIGH)
 {
  rele3=!rele3 ;
 digitalWrite(D0,rele3);
 }
 if(digitalRead(D7) == HIGH)
 {
 rele4=!rele4 ;
 digitalWrite(D5,rele4);
 }
 delay(300);
 }
ConfigServer::~ConfigServer()
{
  stopServer();
  SPIFFS.end();
}

void ConfigServer::startServer() {
  Serial.printf("[ConfigServer]: Starting HTTP server. Waiting for config from app\r\n");
  if (server) stopServer();

  server = new ESP8266WebServer(80);
  server->on("/api/product/configuration", HTTP_POST, [&]() { onConfigure(); });
  server->on("/api/product/info", HTTP_GET, [&]() { onDeviceInfo(); });
  server->begin();
}

void ConfigServer::handleServer() {
  while (!isConfigured) {
    server->handleClient();
    yield();
  }
}

void ConfigServer::stopServer() {
  if (server) {
    Serial.printf("[ConfigServer]: Stopping HTTP server\r\n");
    server->stop();
    delete server;
    server = nullptr;
  }
}

void ConfigServer::onConfigure() {
  Serial.println("[ConfigServer]: receiving configuration\r\n");


  if (server->hasArg("configuration"))
  {
    String decoded = server->arg("configuration");
    String key = String(getChipId32(), HEX);
    
    DynamicJsonDocument jsonConfig(1024);
    deserializeJson(jsonConfig, decoded);
    Serial.println(decoded);
    
    if (saveJsonConfig(jsonConfig)) {
      server->send(200, "application/json", "{\"success\":true}");
      extractJsonConfig(jsonConfig);

      Serial.println("[ConfigServer]: Configuration updated!\r\n");
      isConfigured = true;
    }
    else
    {
      server->send(500, "application/json", "{\"success\":false}");
      delay(1000);
      ESP.restart();
    }
  }
}

void ConfigServer::onDeviceInfo() {
  Serial.printf("[ConfigServer]: handling device Info\r\n");

  DynamicJsonDocument doc(1024);
  String chipId = String(getChipId32(), HEX);
    
  JsonObject productInfo = doc.createNestedObject("productInfo");
  productInfo["type"] = PRODUCT_CODE;
  productInfo["bssid"] = getMacAddress();

  JsonArray devices = doc.createNestedArray("devices");

  JsonObject device1 = devices.createNestedObject();
  device1["name"] = "sw_1";
  device1["code"] = SWITCH_PRODUCT_CODE;

  JsonObject device2 = devices.createNestedObject();
  device2["name"] = "sw_2";
  device2["code"] = SWITCH_PRODUCT_CODE;

  String data = "";
  serializeJson(doc, data);

  server->send(200, "application/json", data);
}

bool ConfigServer::loadJsonConfig()
{
  Serial.printf("[ConfigServer]: Loading config...");
  File configFile = SPIFFS.open(fileName, "r");
  DynamicJsonDocument jsonConfig(1024);
  DeserializationError err = deserializeJson(jsonConfig, configFile);
  
  if (err) {
    Serial.printf("failed!\r\n");
    return false;
  }
  extractJsonConfig(jsonConfig);
  Serial.printf("success\r\n");
  return true;
}

bool ConfigServer::saveJsonConfig(const JsonDocument &doc){
  Serial.printf("[ConfigServer]: Saving config...");
  
  File configFile = SPIFFS.open(fileName, "w");
  
  if (!configFile)
  {
    Serial.printf("failed!!!\r\n");
    return false;
  }
  serializeJson(doc, configFile);
  configFile.close();
  Serial.printf("success\r\n");
  return true;
}

bool ConfigServer::setupWiFi() {
  Serial.printf("[ConfigServer]: Connecting to WiFi");
  
  uint8_t count = 0;
  struct station_config conf;
  wifi_station_get_config(&conf);  
  char* ssid = reinterpret_cast<char*>(conf.ssid);
  char* password = reinterpret_cast<char*>(conf.password);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.printf(".");
    count++;
    if (count > 10) break;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("done. IP is %s\r\n", WiFi.localIP().toString().c_str());
    return true;
  } 
  else
  {
    Serial.printf("failed!\r\n");
    return false;
  } }
void ConfigServer::notifySuccess() {
  WiFiUDP udp;
  IPAddress ipMulti(239, 255, 255, 250);  
  String sendstr = WiFi.localIP().toString() + ";" + getMacAddress();  
  
  for(int i = 0; i < 10; i++){
    udp.beginMulticast(WiFi.localIP(), ipMulti, 1982);
    udp.beginPacketMulticast(ipMulti, 1982, WiFi.localIP());    
    //udp.beginPacket(ipMulti, 1982);
    udp.print(sendstr);
    udp.endPacket();
  }
  
  Serial.println("[ConfigServer]: Notify success!");
}

bool ConfigServer::reconnectToWiFi() {
  Serial.println("[ConfigServer]: Reconnecting to WiFi after SmartConfig..");
  
  // load credentials from NVR and reconnect to WiFi. 
  
  struct station_config conf;
  wifi_station_get_config(&conf);  
  const char* ssid = reinterpret_cast<const char*>(conf.ssid);
  const char* password = reinterpret_cast<const char*>(conf.password);
    
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
    
  WiFi.begin(ssid, password);
  Serial.print("[ConfigServer]: Connecting to "); Serial.print(ssid);
  Serial.print(" with password "); Serial.println(password);
 
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
 
  Serial.println("[ConfigServer]: WiFi Connected");
  Serial.printf("[ConfigServer]: IP is %s\r\n", WiFi.localIP().toString().c_str());

  if(WiFi.localIP().toString() == "0.0.0.0") {
    Serial.println("[ConfigServer]: Got invalid IP address from DHCP..!"); 
    return false;
  } else {
    return true;  
  }
}

void ConfigServer::startSmartConfig() {
  Serial.printf("[ConfigServer]: Entering smartconfig");

  WiFi.disconnect();
  
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  while (1)
  {
    delay(500);
    Serial.printf(".");
    if (WiFi.smartConfigDone())
    {
      WiFi.stopSmartConfig();
      delay(1000);
      break;
    }
    touch();
  }

  Serial.println("[ConfigServer]: Smartconfig done!"); 
  
  bool isConnected = reconnectToWiFi();  

  if(isConnected) {
    notifySuccess();  
  } else {
    Serial.println("[ConfigServer]: Invalid IP address. Erase the ESP and restarting...!"); 
    WiFi.disconnect();   
    WiFi.begin("0","0"); // adding this effectively seems to erase the previous stored SSID/PW
    ESP.restart();
    delay(1000); 
  }  
}
 
String ConfigServer::getMacAddress() {
  byte mac[6];
  WiFi.macAddress(mac);
  String cMac = "";
  for (int i = 0; i < 6; ++i) {
    cMac += String(mac[i],HEX);
    if(i<5)
      cMac += "-";
  }
  cMac.toUpperCase();
  return cMac;
}

uint32_t ConfigServer::getChipId32()
{
  return ESP.getChipId();
}
 
void ConfigServer::extractJsonConfig(const JsonDocument &doc)
{
  strlcpy(config.appKey, doc["credentials"]["appkey"] | "", sizeof(config.appKey));
  strlcpy(config.appSecret, doc["credentials"]["appsecret"] | "", sizeof(config.appSecret));
  strlcpy(config.sw1_id, doc["devices"][0]["id"] | "", sizeof(config.sw1_id));
  strlcpy(config.sw2_id, doc["devices"][1]["id"] | "", sizeof(config.sw2_id));
}

#endif