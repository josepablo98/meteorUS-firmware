#include <WiFi.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>

IPAddress ipHost(192, 168, 100, 88);

const char *mqttServer = "broker.hivemq.com";
const int portMqtt = 1883;

unsigned long lastUpdate = 0;
unsigned long updateInterval = 5000;

bool isOnLast = false;
bool isOn = false;
bool isHot = false;
bool isCold = false;
bool firstRun = true;
bool receivedMQTTMessage = false;

#define LEDGOODPIN 4
#define LEDWIFIPIN 16
#define LEDCOLDPIN 17
#define DHTPIN 18
#define I2C_ADDRESS 0x77
#define BUZZERPIN 19
#define LEDHOTPIN 23
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085 bmp;
WiFiClient espClient;
PubSubClient client(espClient);

uint64_t chipid = ESP.getEfuseMac();
String sensorId1 = String(chipid) + "_1";
String sensorId2 = String(chipid) + "_2";

void callback(char *topic, byte *payload, unsigned int length) {
  char messageTemp[length + 1];
  memcpy(messageTemp, payload, length);
  messageTemp[length] = '\0';

  String message = String(messageTemp);

  if (strcmp(topic, "setIsOn") == 0) {
    isOn = strcmp(message.c_str(), "true") == 0;
  }
  else if (strcmp(topic, "setIsHot") == 0) {
    isHot = strcmp(message.c_str(), "true") == 0;
  }
  else if (strcmp(topic, "setIsCold") == 0) {
    isCold = strcmp(message.c_str(), "true") == 0;
  }

  receivedMQTTMessage = true;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(String(chipid).c_str())) {
      Serial.println("Connected");
      client.subscribe("setIsOn");
      client.subscribe("setIsHot");
      client.subscribe("setIsCold");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void httpPostTempHum(float hum, float temp, uint64_t chipid, String sensorId) {
  HTTPClient http;
  http.begin("http://" + ipHost.toString() + ":80/meteorUS/temphum");
  http.addHeader("Content-Type", "application/json");
  String httpRequestData = "{\"temperature\":" + String(temp) + ",\"humidity\":" + String(hum) + ",\"boardId\":" + String(chipid) + ",\"sensorId\":\"" + sensorId + "\"}";
  http.POST(httpRequestData);
  http.end();
}

void httpPostActuator(bool isOn, bool isHot, bool isCold, uint64_t chipid, String sensorId) {
  HTTPClient http;
  http.begin("http://" + ipHost.toString() + ":80/meteorUS/actuator");
  http.addHeader("Content-Type", "application/json");
  String isOnToString = isOn ? "true" : "false";
  String isHotToString = isHot ? "true" : "false";
  String isColdToString = isCold ? "true" : "false";
  String httpRequestData = "{\"isOn\":" + isOnToString + ",\"isHot\":" + isHotToString + ",\"isCold\":" + isColdToString + ",\"boardId\":" + String(chipid) + ",\"sensorId\":\"" + sensorId + "\"}";
  http.POST(httpRequestData);
  http.end();
}

void httpPostPressure(float press, float altitude, uint64_t chipid, String sensorId) {
  HTTPClient http;
  http.begin("http://" + ipHost.toString() + ":80/meteorUS/pressure");
  http.addHeader("Content-Type", "application/json");
  String httpRequestData = "{\"pressure\":" + String(press) + ",\"altitude\":" + String(altitude) + ",\"boardId\":" + String(chipid) + ",\"sensorId\":\"" + sensorId + "\"}";
  http.POST(httpRequestData);
  http.end();
}
void launchWiFiManager() {
  WiFiManager wifiManager;
  digitalWrite(LEDWIFIPIN, HIGH);
  if (!wifiManager.autoConnect("meteorUS")) {
    Serial.println("Failed to connect, please connect to the 'AutoConnectAP' network, open a browser and navigate to 192.168.4.1 to enter WiFi credentials.");
    delay(3000);
    ESP.restart();
  }
}

void playHotAlarm() {
  digitalWrite(LEDGOODPIN, LOW);
  unsigned long startMillis = millis();
  while (millis() - startMillis < 10000) {
    for (int i = 0; i < 350; i++) {
      digitalWrite(BUZZERPIN, HIGH);
      digitalWrite(LEDHOTPIN, HIGH);
      delay(1);
      digitalWrite(BUZZERPIN, LOW);
      delay(1);
    }
    delay(50);
    for (int i = 0; i < 150; i++) {
      digitalWrite(BUZZERPIN, HIGH);
      digitalWrite(LEDHOTPIN, LOW);
      delay(2);
      digitalWrite(BUZZERPIN, LOW);
      delay(2);
    }
  }
  digitalWrite(LEDHOTPIN, HIGH);
}

void playColdAlarm() {
  digitalWrite(LEDGOODPIN, LOW);
  unsigned long startMillis = millis();
  while (millis() - startMillis < 10000) {
    for (int i = 0; i < 350; i++) {
      digitalWrite(BUZZERPIN, HIGH);
      digitalWrite(LEDCOLDPIN, HIGH);
      delay(1);
      digitalWrite(BUZZERPIN, LOW);
      delay(1);
    }
    delay(50);
    for (int i = 0; i < 150; i++) {
      digitalWrite(BUZZERPIN, HIGH);
      digitalWrite(LEDCOLDPIN, LOW);
      delay(2);
      digitalWrite(BUZZERPIN, LOW);
      delay(2);
    }
  }
  digitalWrite(LEDCOLDPIN, HIGH);
}