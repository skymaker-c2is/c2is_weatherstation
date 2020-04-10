# C2IS Weatherstation

*Early developement. Not tested yet.*

This is another firmware solution for the famous "Solar Powered WiFi Weather Station" from Open Green Energy: https://www.instructables.com/id/Solar-Powered-WiFi-Weather-Station-V20/

Since I don't like "cloud"-services, I wanted to host everything on my own server and ended up with a mosquitto + Telegraf + InfluxDB + Grafana stack. This is the firmware part. It features OTA updates and device-specific configuration stored in the SPIFFS as a json file.

I replaced the BME280 with a GY-21P shield, which has a BMP280 and a Si7021 on it. My first setup with the BME280 showed me 100 % humidity after a few weeks this winter. The Si7021 has a thin cover, which protects it from corrosion.

# Requirements

- git-describe-arduino (https://github.com/fabiuz7/git-describe-arduino)
- ArduinoJson 6
- PubSubClient
- Adafruit_BMP280
- HTU21D (https://github.com/enjoyneering/HTU21D)

# ToDo

- Compability to BME280-only setups
- OTA updates for several boards (only D1 mini Pro is supported right now)
- Documentation
