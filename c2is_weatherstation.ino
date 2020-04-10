/* 
 *  The C2IS-way to know your outside temperature, humidity and air pressure.
 *  Have a look at the README.
 */


/* 
 *  Remove this line if you don't want to use OTA updates.
 *  If you want to use OTA updates, git versioning is needed.
 *  Have a look here: https://github.com/fabiuz7/git-describe-arduino
 */
#include "src/git-version.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <HTU21D.h>

#define MQTT_MAX_PACKET_SIZE = 512; // JSON is biiig.

struct Config {
  char deviceName[64];
  char wifiSsid[64];
  char wifiPassword[64];
  char mqttServer[64];
  int mqttPort;
  char mqttTopic[64];
  char updateServer[64];
  int updatePort;
  char updatePath[64];
};

struct Measurements {
  float humidity;
  float pressure;
  float temperature;
  float temperature_si;
  int rssi;
  float battery;
};

Measurements measurements;
Config config;

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BMP280 bmp;
HTU21D si7021(HTU21D_RES_RH12_TEMP14);

// Loads the configuration from /config.json
// This is just taken from the ArduinoJson Documentation.
bool loadConfiguration() {
  // Open file for reading
  File file = SPIFFS.open("/config.json", "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = JSON_OBJECT_SIZE(8) + 220;
  StaticJsonDocument<capacity> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("[config] Failed to read file, using default configuration"));
    return false;
  }

  // Copy values from the JsonDocument to the Config
  strlcpy(config.deviceName,
          doc["deviceName"] | "Weatherstation",
          sizeof(config.deviceName));
  strlcpy(config.wifiSsid,
          doc["wifiSsid"],
          sizeof(config.wifiSsid));
  strlcpy(config.wifiPassword,
          doc["wifiPassword"],
          sizeof(config.wifiPassword));
  strlcpy(config.mqttServer,
          doc["mqttServer"],
          sizeof(config.mqttServer));
  config.mqttPort = doc["mqttPort"] | 1883;
  strlcpy(config.mqttTopic,
          doc["mqttTopic"],
          sizeof(config.mqttTopic));
  strlcpy(config.updateServer,
          doc["updateServer"],
          sizeof(config.updateServer));
  config.updatePort = doc["updatePort"] | 80;
  strlcpy(config.updatePath,
          doc["updatePath"],
          sizeof(config.updatePath));

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
  return true;
}

// Setup Wifi connection.
void setup_wifi() {
  delay(10);
  Serial.print("[wifi] Connecting to ");
  Serial.println(config.wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSsid, config.wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("[wifi] Wifi connected");
  Serial.println("[wifi] IP address: ");
  Serial.println(WiFi.localIP());
}

// Reconnect helper function if we loose connection before sending measurements.
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("[mqtt] Attempting MQTT connection...");
    if (client.connect(config.deviceName)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void check_updates() {
  t_httpUpdate_return ret = ESPhttpUpdate.update(config.updateServer, config.updatePort, config.updatePath, GIT_VERSION);
  switch(ret) {
    case HTTP_UPDATE_FAILED:
        Serial.println("[update] Update failed.");
        break;
    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("[update] Update no Update.");
        break;
    case HTTP_UPDATE_OK:
        Serial.println("[update] Update ok."); // may not be called since we reboot the ESP
        break;
  }
}

void getSensorData() {
  delay(100);
  measurements.temperature = bmp.readTemperature();
  measurements.pressure = bmp.readPressure();
  measurements.humidity = si7021.readHumidity();
  measurements.temperature_si = si7021.readTemperature(SI70xx_TEMP_READ_AFTER_RH_MEASURMENT);
  measurements.rssi = WiFi.RSSI();
  measurements.battery = (analogRead(A0) / 1023.0 * 4.2);

  // Heater disabled.
  si7021.setHeater(HTU21D_OFF);
  si7021.setHeaterLevel(0x00);

  /* This just serves as an example how to control the integrated heater of the Si7021.
  if((humidity >= 80.0 && humidity < 90.0) || temperature_si < 1.0) {
      si7021.setHeaterLevel(0x00);
      si7021.setHeater(HTU21D_ON);
  }
  if(humidity >= 90.0) {
      si7021.setHeaterLevel(0x01);
      si7021.setHeater(HTU21D_ON);
  }*/
  
  Serial.print("[sensor] Temperature: ");
  Serial.println(measurements.temperature);
  Serial.print("[sensor] Pressure: ");
  Serial.println(measurements.pressure);
  Serial.print("[sensor] Temperature Si: ");
  Serial.println(measurements.temperature_si);
  Serial.print("[sensor] Humidity: ");
  Serial.println(measurements.humidity);
  //Serial.print("[sensor] Heater Status: ");
  //Serial.println(si7021.getHeater() ? "ON" : "OFF");
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output, so we can see if the D1 is turned on.
  Serial.begin(9600);
  if (!SPIFFS.begin()) {
    Serial.println("[config] Failed to mount filesystem");
    return;
  }

  if (!loadConfiguration()) {
    return;
  } else {
    Serial.println("[config] Config loaded");
  }

  setup_wifi();
  check_updates();

  // initialize PubSubClient
  client.setServer(config.mqttServer, config.mqttPort);

  if (!client.connected()) {
    reconnect();
  }

  // initialize BMP280
  bmp.begin(0x76);
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X1,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X1,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_OFF,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time, doesn't matter here, because we're using FORCED mode. */

  // initialize Si7021
  si7021.begin();

  getSensorData();

  const size_t capacity = JSON_OBJECT_SIZE(7);
  DynamicJsonDocument doc(capacity);

  doc["station"] = config.deviceName;
  doc["temperature"] = measurements.temperature;
  doc["temperature_si"] = measurements.temperature_si;
  doc["humidity"] = measurements.humidity;
  doc["pressure"] = measurements.pressure;
  doc["rssi"] = measurements.rssi;
  doc["battery"] = measurements.battery;

  char buffer[512];
  serializeJson(doc, buffer);

  client.publish(config.mqttTopic, buffer);
  delay(20);
  client.disconnect();
  Serial.println("Messages sent. Going to sleep...");
  ESP.deepSleep(300e6);
}

void loop() {
  // put your main code here, to run repeatedly:

}
