#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <TimeLib.h>
#include <NtpClientLib.h>

#include <BME280I2C.h>

#define SERIAL_BAUD 115200

//WIFI
const char* ssid = "wifi-ssid";
const char* pass = "password";

//MQTT
const char* mqtt_server = "xxx.xxx.xxx.xxxx";
const int mqtt_port = 1883;
const char* mqtt_user = "user";
const char* mqtt_pass = "pass";
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

//BME280
bool metric = true;
//NTP
int8_t timeZone = 5;
//Init all
BME280I2C bme;
WiFiClient wclient;
PubSubClient client(mqtt_server, mqtt_port, callback, wclient);

long lastSync = 0;
long syncInterval = 30 * 1000;
// void getTime() {}

float collTemp = 0.0;
float collHum = 0.0;
float collPres = 0.0;
int count = 0;

void connect() {
  //Wifi: Inform
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(".");
  //Wifi: init Connecting
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
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

  //MQTT: Inform
  Serial.print("Connecting to MQTT server.");
  //MQTT: init connecting
  client.connect("espMeteo", mqtt_user, mqtt_pass);
  while(!client.connected()) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to MQTT server");

  NTP.begin("pool.ntp.org", timeZone, false);
  NTP.setInterval(63);
}

// void sensors() {
//   Serial.println();
// }

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
  Serial.println(" torr");
}

void publish (JsonObject &root) {
  char jsonChar[100];
  root.printTo(jsonChar);
  client.publish("bedroom/meteo", jsonChar);
}

void getTemp() {
  float temp(NAN), hum(NAN), pres(NAN);
  // unit: B000 = Pa, B001 = hPa, B010 = Hg, B011 = atm, B100 = bar, B101 = torr, B110 = N/m^2, B111 = psi
  uint8_t pressureUnit(5);
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
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {} // Wait
  //Just for cool output
  delay(1000);
  Serial.print("MeteoStation v.1.0.0");
  while (!bme.begin()) {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
  connect();
}

void loop() {
  bool toReconnect = false;

  //Wifi: check Connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    toReconnect = true;
  }
  if (!client.connected()) {
    Serial.println("MQTT disconnected");
    toReconnect = true;
  }

  if (toReconnect) {
    connect();
  }

  client.loop();

  unsigned long currentMillis = millis();
  getTemp();
  if (currentMillis - lastSync > syncInterval) {
    lastSync = currentMillis;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    prepare(root);
    show(root);
    publish(root);
    resetValues();
  }

  delay(500);
}
