/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <vector>

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

unsigned long lastBroadcastTime = 0; // Timer for broadcasting temperature
const unsigned long broadcastInterval = 5000; // Broadcast every 5 seconds

std::vector<String> tempLog; // List to store temperature readings

const int oneWireBus = 4;   

OneWire oneWire(oneWireBus);

DallasTemperature sensors(&oneWire);

// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

//Defining pins
#define LED_PIN 19
#define BTN_PIN 35

//Button related
unsigned long lastCheckTime = 0;
unsigned long ledOnTime = 0;
int holdSeconds = 0;
bool ledOn = false;
bool alreadyActivated = false;

//

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

String temp() {
  sensors.requestTemperatures(); 
  float celsius = sensors.getTempCByIndex(0);
  Serial.print("Celsius: ");
  Serial.println(celsius);
  return String(celsius, 2) + " °C"; // Format temperature to 1 decimal place
}

// Read File from LittleFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Initialize WiFi
bool initWiFi() {
  if(ssid==""){
    Serial.println("Undefined SSID.");
    return false;
  }

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

void notifyClients() {
  String temperature = temp();
  ws.textAll(temperature); // Send temperature to all clients
  tempLog.push_back(temperature); // Log the temperature
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "update") == 0) {
      Serial.println("Update request received from client.");
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "TEMP") {
    return temp();
  }
  
  return String();
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  sensors.begin();

  initLittleFS();

  // Set GPIO 2 as an OUTPUT
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  //LED_PIN setup
  pinMode(LED_PIN, OUTPUT);

  //BTN_PIN setup
  pinMode(BTN_PIN, INPUT_PULLUP);
  
  // Load values saved in LittleFS
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  if(initWiFi()) {
    initWebSocket();
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", LittleFS, "/");

    // Route to set GPIO state to HIGH
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, HIGH);
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });

    // Route to set GPIO state to LOW
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, LOW);
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });
    server.begin();
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", LittleFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
}

void loop() {
  ws.cleanupClients();
  
  // Broadcast temperature to all clients every 5 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - lastBroadcastTime >= broadcastInterval) {
    lastBroadcastTime = currentMillis;
    notifyClients(); // Send temperature update to all connected clients
  }

  unsigned long currentTime = millis();
  digitalWrite(LED_PIN, LOW);

  if (!alreadyActivated) {
    if (currentTime - lastCheckTime >= 1000) {
      lastCheckTime = currentTime;

      int buttonState = digitalRead(BTN_PIN);

      if (buttonState == HIGH) {
        holdSeconds++;
        Serial.print("Button held for ");
        Serial.print(holdSeconds);
        Serial.println(" second(s)");

        if (holdSeconds >= 10 && !ledOn) {
          digitalWrite(LED_PIN, HIGH);
          ledOn = true;
          alreadyActivated = true;
          ledOnTime = currentTime;
          writeFile(LittleFS, passPath, "");
          writeFile(LittleFS, ssidPath, "");

          String newPassword = readFile(LittleFS, passPath);
          String newSSID = readFile(LittleFS, ssidPath);

          Serial.println(newPassword);
          Serial.println(newSSID);

          Serial.println("LED turned ON after 10 seconds hold");
          ESP.restart();
        }
      } else {
        if (holdSeconds > 0) {
          Serial.println("Button released — timer reset");
        }
        holdSeconds = 0;
      }
    }
  }
  if (ledOn && (millis() - ledOnTime >= 5000)) {
    digitalWrite(LED_PIN, LOW);
    ledOn = false;
    Serial.println("LED turned OFF after 5 seconds");
  }
}