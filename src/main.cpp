#include "header.h"



void setup() {
  Serial.begin(9600);
  delay(4000);

  dht.begin();
  Wire.begin();
  if (!bmp.begin()) {
     Serial.println(F("Could not find a valid BMP085 sensor, check wiring!"));
     //while (1);
  } else {
    Serial.println(F("Find a valid BMP sensor"));
  }

  pinMode(LEDHOTPIN, OUTPUT);
  pinMode(LEDCOLDPIN, OUTPUT);
  pinMode(LEDWIFIPIN, OUTPUT);
  pinMode(LEDGOODPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);

  digitalWrite(LEDHOTPIN, LOW);
  digitalWrite(LEDCOLDPIN, LOW);
  digitalWrite(LEDWIFIPIN, LOW);
  digitalWrite(LEDGOODPIN, HIGH);

  if (WiFi.status() != WL_CONNECTED) {
    launchWiFiManager();
  }

  Serial.println("Connected to WiFi");
  digitalWrite(LEDWIFIPIN, LOW);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://" + ipHost.toString() + ":8080/meteorUS/board/" + String(chipid));
    int httpResponseCode = http.GET();
    http.end();
    Serial.print("Geteando board:");
    Serial.println(httpResponseCode);
    if (httpResponseCode == 404) {
      http.begin("http://" + ipHost.toString() + ":8080/meteorUS/board");
      http.addHeader("Content-Type", "application/json");
      String httpRequestData = "{\"boardId\":" + String(chipid) + "}";
      int httpResponseCode1 = http.POST(httpRequestData);
      Serial.print("Board a la BD:");
      Serial.println(httpResponseCode1);
      http.end();
    }

    http.begin("http://" + ipHost.toString() + ":8080/meteorUS/actuator/" + String(chipid) + "/" + sensorId1);
    httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      JsonArray array = doc.as<JsonArray>();
      JsonObject lastActuator = array[array.size() - 1];
      isOn = lastActuator["isOn"];
    }

    http.end();

    EEPROM.begin(8);
    maxTemp = EEPROM.readFloat(0) != 0 ? EEPROM.readFloat(0) : 35.0;
    minTemp = EEPROM.readFloat(4) != 0 ? EEPROM.readFloat(4) : 5.0;

    client.setServer(mqttServer, portMqtt);
    client.setCallback(callback);
    
    Serial.print("Maxtemp: ");
    Serial.println(maxTemp);
    Serial.print("MinTemp: ");
    Serial.println(minTemp);
    EEPROM.end();
  }
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Launching WiFi Manager...");
    launchWiFiManager();
    digitalWrite(LEDWIFIPIN, LOW);
  }

  if (!client.connected()) {
    Serial.println("MQTT connection lost. Reconnecting...");
    reconnect();
  }

  if (receivedMQTTMessage) {
    Serial.println("Received MQTT message.");
    receivedMQTTMessage = false;
  }

  

  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  float pressure = bmp.readPressure() / 100;
  float altitude = bmp.readAltitude();

  Serial.println(temp);
  Serial.println(hum);
  Serial.println(pressure);
  Serial.println(altitude);
  delay(5000);
  httpPostTempHum(hum, temp, chipid, sensorId1);
  httpPostPressure(pressure, altitude, chipid, sensorId2);
  client.loop();

  if (((temp > maxTemp) || (temp < minTemp)) && (!isOn || firstRun)) {
    isOn = true;
    firstRun = false;
    digitalWrite(LEDGOODPIN, LOW);
    if (temp > maxTemp) {
      isHot = true;
      isCold = false;
      digitalWrite(LEDCOLDPIN, LOW);
      playHotAlarm();
      digitalWrite(LEDHOTPIN, HIGH);
    } else {
      isHot = false;
      isCold = true;
      digitalWrite(LEDHOTPIN, LOW);
      playColdAlarm();
      digitalWrite(LEDCOLDPIN, HIGH);
      }
    httpPostActuator(isOn, isHot, isCold, chipid, sensorId1);
  } else if ((temp <= maxTemp) && (temp >= minTemp)) {
      if (isOn) {
        digitalWrite(LEDCOLDPIN, LOW);
        digitalWrite(LEDHOTPIN, LOW);
        digitalWrite(LEDGOODPIN, HIGH);
        isOn = false;
        isHot = false;
        isCold = false;
        httpPostActuator(isOn, isHot, isCold, chipid, sensorId1);
      }
    }
}
