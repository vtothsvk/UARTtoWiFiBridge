#include <M5StickCPlus.h>
//#include <M5StickC.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include "WebServer.h"
#include <Preferences.h>
//#include "DHT12.h" 
#include <Wire.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "newAuth.h"
#include "time.h"
#include "esp_err.h"

void decode(char* data);
esp_err_t event(char* body, char* jwt);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//RTC_DATA_ATTR int bootCount = 0;
//RTC_DATA_ATTR int cFails = 0;
//RTC_DATA_ATTR long timestamp = 1607962136;

const char* serverName = "https://fei.edu.r-das.sk:51415/api/v1/Auth";

char payload[800];
long cStart;

authHandler auth;

const IPAddress apIP(192, 168, 4, 1);
char* apSSID = "UARTtoWiFiBridge";
boolean settingMode;
String ssidList;
String wifi_ssid;
String wifi_password;

bool needConnect = true;

// DNSServer dnsServer;
WebServer webServer(80);

// wifi config store
Preferences preferences;

HTTPClient http;

static char dataBuffer[200];

void wifiConfig(){
  preferences.begin("wifi-config");
  delay(10);
    if (restoreConfig()) {
        if (checkConnection()) { 
        settingMode = false;
        startWebServer();
        return;
        }
    }
    settingMode = true;
    setupMode();

    while (needConnect){
        if (settingMode) {
        }
        webServer.handleClient();
    }
}

void setup(){
    M5.begin();
    //M5.Axp.ScreenBreath(0);
    //Wire.begin();
    //Serial init
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 36, 26);
    //Wire.begin(0, 26);
    /*
    WiFi.begin(ssid, pass);
    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
    }
    Serial.println();
    */
    
    wifiConfig();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    /*
    while ((WiFi.status() != WL_CONNECTED)&&((millis() - start) <= cTime)) {
        delay(100);
        Serial.print(".");
    }//while
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        ++cFails;
        esp_sleep_enable_timer_wakeup(cFails * cSleepTime * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
    }//if (WiFi.status() != WL_CONNECTED)
    */

   /*
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
    */
  
  ArduinoOTA
  .onStart([]() {
  String type;
  if (ArduinoOTA.getCommand() == U_FLASH)
    type = "sketch";
  else // U_SPIFFS
    type = "filesystem";

  // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  Serial.println("Start updating " + type);
  })
    
  .onEnd([]() {
  Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.printf("Progress: %u%%     \r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
  Serial.printf("Error[%u]: ", error);
  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}//setup

void loop(){
  //float bat = M5.Axp.GetBatVoltage();

  if (M5.BtnA.wasPressed()) {
    Serial.println("Blink Motherfucker!");
    OTAhandler();
  }

  int i = 0;

  while (Serial2.available()) {
    dataBuffer[i] = Serial2.read();
    i++;
  }

  if (i) {
    Serial.println("Data Callback");
    if (strlen(dataBuffer) > 10) {
      decode(dataBuffer);
    } else {
      Serial.printf("No data");
    }
    Serial2.flush();
    memset(dataBuffer, '\0', sizeof(dataBuffer));
  }

  M5.update();
  delay(10);
}//loop

boolean restoreConfig() {
  wifi_ssid = preferences.getString("WIFI_SSID");
  wifi_password = preferences.getString("WIFI_PASSWD");
  Serial.print("WIFI-SSID: ");
  //M5.Lcd.print("WIFI-SSID: ");
  Serial.println(wifi_ssid);
  //M5.Lcd.println(wifi_ssid);
  Serial.print("WIFI-PASSWD: ");
  //M5.Lcd.print("WIFI-PASSWD: ");
  Serial.println(wifi_password);
  //M5.Lcd.println(wifi_password);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  if (wifi_ssid.length() > 0) {
    return true;
} else {
    return false;
  }
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  M5.Lcd.println("Waiting for Wi-Fi connection");
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      M5.Lcd.println("Connected!");
      Serial.println("Connected!");
      //M5.Lcd.println("Connected!");
      needConnect = false;
      return (true);
    }
    delay(500);
    Serial.print(".");
    //M5.Lcd.print(".");
    count++;
  }
  Serial.println("Timed out.");
  //M5.Lcd.println("Timed out.");
  return false;
}

void startWebServer() {
  if (settingMode) {
    Serial.print("Starting Web Server at ");
    //M5.Lcd.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    //M5.Lcd.println(WiFi.softAPIP());
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    webServer.on("/setap", []() {
      //String ssid = urlDecode(webServer.arg("ssid"));
      String ssid = webServer.arg("ssid");
      Serial.print("SSID: ");
      //M5.Lcd.print("SSID: ");
      Serial.println(ssid);
      //M5.Lcd.println(ssid);
      //String pass = urlDecode(webServer.arg("pass"));
      String pass = webServer.arg("pass");
      Serial.print("Password: ");
      //M5.Lcd.print("Password: ");
      Serial.println(pass);
      //M5.Lcd.println(pass);
      Serial.println("Writing SSID to EEPROM...");
      //M5.Lcd.println("Writing SSID to EEPROM...");

      // Store wifi config
      Serial.println("Writing Password to nvr...");
      //M5.Lcd.println("Writing Password to nvr...");
      preferences.putString("WIFI_SSID", ssid);
      preferences.putString("WIFI_PASSWD", pass);

      Serial.println("Write nvr done!");
      //M5.Lcd.println("Write nvr done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      delay(3000);
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else {
    Serial.print("Starting Web Server at ");
    //M5.Lcd.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    //M5.Lcd.println(WiFi.localIP());
    webServer.on("/", []() {
      String s = "<h1>STA mode</h1><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("STA mode", s));
    });
    webServer.on("/reset", []() {
      // reset the wifi config
      preferences.remove("WIFI_SSID");
      preferences.remove("WIFI_PASSWD");
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
      delay(3000);
      ESP.restart();
    });
  }
  webServer.begin();
}

void setupMode() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  //M5.Lcd.println("");
  for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  WiFi.mode(WIFI_MODE_AP);
  // WiFi.softAPConfig(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
  // WiFi.softAP(const char* ssid, const char* passphrase = NULL, int channel = 1, int ssid_hidden = 0);
  // dnsServer.start(53, "*", apIP);
  startWebServer();
  Serial.print("Starting Access Point at \"");
  //M5.Lcd.print("Starting Access Point at \"");
  Serial.print(apSSID);
  //M5.Lcd.print(apSSID);
  Serial.println("\"");
  //M5.Lcd.println("\"");
  M5.Lcd.println("No WiFi Connection!");
}

String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

void OTAhandler(){
  M5.Axp.ScreenBreath(255);
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.println("Preparing OTA");
  delay(5000);
  M5.update();
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.println("             ");
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.printf("Waiting for\nFW");

  while (true) {
    ArduinoOTA.handle();

    if(M5.BtnA.wasPressed()) {
      Serial.printf("End OTA\r\n");
      M5.Lcd.setCursor(0, 1);
      M5.Lcd.println("           ");
      M5.Axp.ScreenBreath(0);
      return;
    }
    M5.update();
  }
}

void decode(char* data){
  Serial.printf("Received data: %s\r\n", data);

  char* events = strtok(data, "#");
  char* rest = strtok(NULL, "\0");
  
  int noEvents = 0;
  sscanf(events, "%d", &noEvents);

  //Serial.printf("no str: %s\r\nno: %d\r\ndata: %s\r\n", events, noEvents, rest);
  
  char body[1024];

  struct tm mytime;
  getLocalTime(&mytime);
  time_t epoch = mktime(&mytime);
  //long epoch = 1611751042;
  char epoch_str[11];
  sprintf(epoch_str, "%ld", (long)epoch);

  if (noEvents) {
  //if (0) {
    char* logger = strtok(rest, "#");
    sprintf(body, "[{\"LoggerName\": \"");
    strcat(body, logger);
    strcat(body, "\", \"Timestamp\": ");
    strcat(body, epoch_str);
    strcat(body, ", \"MeasuredData\": [");

    char* events[6];
    char* values[6];

    for(uint8_t i = 0; i < noEvents; i++) {
      events[i] = strtok(NULL, "#");
      values[i] = strtok(NULL, "#");

      strcat(body, "{\"Name\": \"");
      strcat(body, events[i]);
      strcat(body, "\", \"Value\": ");
      strcat(body, values[i]);
      strcat(body, "}");

      if (i < noEvents - 1) {
        strcat(body, ",");
      }//if (i < noEvents - 1)
    }//for(uint8_t i = 0; i < noEvents; i++)

    strcat(body, "], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"");
    strcat(body, myId);
    strcat(body, "\"}]");

    Serial.printf("Request payload:\r\n%s\r\n", body);

  } else {
    Serial.println("Unknown event...");
  }//if (noEvents)

  //create jwt
  char jwt[500] = "Bearer ";
  size_t jlen;
  auth.createJWT((uint8_t*)jwt + strlen(jwt), sizeof(jwt) - strlen(jwt), &jlen, (long)epoch);

  Serial.printf("\r\nJWT:\r\n%s\r\n\r\n", jwt);
  
  event(body, jwt);

}//decode

esp_err_t event(char* body, char* jwt) {

  struct tm mytime;
    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", jwt);

    int ret = http.POST(body);
    //kontrola responsu
    if(ret != 200){
      Serial.printf("ret: %d\r\n", ret);
      Serial.printf("response: %s\r\n", http.getString().c_str());
      return ESP_FAIL;
    } else {
      Serial.println("OK");
    }

    //koniec
    http.end();

  return ESP_OK;
}
