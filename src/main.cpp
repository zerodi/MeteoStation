/**
* MeteoStation
* Based on esp8266 (ModeMCU v1.0) and BME280
*/
#include <Arduino.h>

//Sensor libs
#include <Wire.h>
#include <BME280I2C.h>

//Cool stuff for sending json
#include <ArduinoJson.h>

//Main web libs
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//Work with mornal time, which we get from ntp server
#include <TimeLib.h>
#include <NtpClientLib.h>

//WIFI
const char* wifi_ssid = "ssid";
const char* wifi_password = "password";

//MQTT
const char* mqtt_server = "x.x.x.x";
const int mqtt_port = 1883;
const char* mqtt_user = "user";
const char* mqtt_password = "password";
const char* mqtt_topic = "esp/meteo";


//Init all
WiFiClient wificlient;
PubSubClient client(wificlient);
BME280I2C bme;

int8_t timeZone = 5; //Your timeZone
bool metric = true; //Set your metric system
long lastSync = 0;
long lastGet = 0;
long syncInterval = 600 * 1000;
long getTempInterval = 60 * 1000;
float collTemp = 0.0;
float collHum = 0.0;
float collPres = 0.0;
int count = 0;

void connect_wifi() {
  //Wifi: Inform
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid);
  //Wifi: init Connecting
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  //Wifi: wait until connected
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  //Wifi: show connection info
  Serial.println("Wifi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  NTP.begin("pool.ntp.org", timeZone, false);
  NTP.setInterval(63);
}

void connect_mqtt() {
  while(!client.connected()) {
    Serial.print("Connecting to MQTT server...");
    if (client.connect("espMeteo", mqtt_user, mqtt_password)) {
      Serial.println("\tConnected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void show(JsonObject &root) {
  Serial.print(root.get<String>("time"));
  Serial.print("\t\tTemp: ");
  Serial.print(root.get<String>("temperature"));
  Serial.print("°" + String(metric ? 'C' : 'F'));
  Serial.print("\t\tHumidity: ");
  Serial.print(root.get<String>("humidity"));
  Serial.print("% RH");
  Serial.print("\t\tPressure: ");
  Serial.print(root.get<String>("pressure"));
  Serial.println(" bar");
}

void publish (JsonObject &root) {
  char jsonChar[100];
  root.printTo(jsonChar);
  client.publish(mqtt_topic, jsonChar);
}

void getTemp() {
  float temp(NAN), hum(NAN), pres(NAN);
  // unit: B000 = Pa, B001 = hPa, B010 = Hg, B011 = atm, B100 = bar, B101 = torr, B110 = N/m^2, B111 = psi
  uint8_t pressureUnit(4);
  // Parameters: (float& pressure, float& temp, float& humidity, bool celsius = false, uint8_t pressureUnit = 0x0)
  bme.read(pres, temp, hum, metric, pressureUnit);
  collTemp += temp;
  collHum += hum;
  collPres += pres;
  count++;
}

void prepare(JsonObject &root) {
  root.set("time", NTP.getTimeDateString());
  root.set("temperature", String(collTemp / count));
  root.set("pressure", String(collPres / count));
  root.set("humidity", String(collHum / count));
}

void resetValues() {
  count = 0;
  collTemp = 0.0;
  collPres = 0.0;
  collHum = 0.0;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {} // Wait
  while (!bme.begin()) {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
  connect_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  bool toReconnect = false;

  //Wifi: check Connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    connect_wifi();
  }
  if (!client.connected()) {
    Serial.println("MQTT disconnected");
    connect_mqtt();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastGet > getTempInterval) {
    lastGet = currentMillis;
    getTemp();
  }
  if (currentMillis - lastSync > syncInterval) {
    lastSync = currentMillis;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    prepare(root);
    show(root);
    publish(root);
    resetValues();
  }
}
