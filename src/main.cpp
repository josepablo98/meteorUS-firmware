#include "header.h"

void setup() {
  Serial.begin(9600);
  delay(4000);

  dht.begin();
  Wire.begin();
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP085 sensor, check wiring!"));
  }
  else {
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
    http.begin("http://" + ipHost.toString() + ":80/meteorUS/board/" + String(chipid));
    int httpResponseCode = http.GET();
    http.end();
    Serial.print("Geteando board:");
    Serial.println(httpResponseCode);
    if (httpResponseCode == 404) {
      http.begin("http://" + ipHost.toString() + ":80/meteorUS/board");
      http.addHeader("Content-Type", "application/json");
      String httpRequestData = "{\"boardId\":" + String(chipid) + "}";
      int httpResponseCode1 = http.POST(httpRequestData);
      Serial.print("Board a la BD:");
      Serial.println(httpResponseCode1);
      http.end();
    }

    http.begin("http://" + ipHost.toString() + ":80/meteorUS/actuator/" + String(chipid) + "/" + sensorId1);
    httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      JsonArray array = doc.as<JsonArray>();
      JsonObject lastActuator = array[array.size() - 1];
      isOnLast = lastActuator["isOn"];
    }

    http.end();

    client.setServer(mqttServer, portMqtt);
    client.setCallback(callback);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Launching WiFi Manager...");
    launchWiFiManager();
    digitalWrite(LEDWIFIPIN, LOW);
  }

  if (!client.connected())
    Serial.println("MQTT connection lost. Reconnecting...");
    reconnect();
  }

  if (receivedMQTTMessage) {
    Serial.println("Received MQTT message.");
    receivedMQTTMessage = false;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate > updateInterval) {
    lastUpdate = currentMillis;
    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    float pressure = bmp.readPressure() / 100;
    float altitude = bmp.readAltitude();

    Serial.println(temp);
    Serial.println(hum);
    Serial.println(pressure);
    Serial.println(altitude);
    httpPostTempHum(hum, temp, chipid, sensorId1);
    httpPostPressure(pressure, altitude, chipid, sensorId2);
    if (isOn && (!isOnLast || firstRun)) {
      httpPostActuator(isOn, isHot, isCold, chipid, sensorId1);
      if (isHot) {
        playHotAlarm();
      }
      else {
        playColdAlarm();
      }
      isOnLast = true;
      firstRun = false;
    }
    else if (!isOn && isOnLast && !firstRun) {
      digitalWrite(LEDHOTPIN, LOW);
      digitalWrite(LEDCOLDPIN, LOW);
      httpPostActuator(isOn, isHot, isCold, chipid, sensorId1);
      isOnLast = false;
      digitalWrite(LEDGOODPIN, HIGH);
      firstRun = false;
    }
  }
  client.loop();
}
