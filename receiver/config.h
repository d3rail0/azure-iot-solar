#ifndef CONFIG_H
#define CONFIG_H
// Physical device information for board and sensor
const char* DEVICE_ID = "Sensor" //"Feather HUZZAH ESP8266 WiFi"

// Interval time(ms) for sending message to IoT Hub
#define INTERVAL 2000

// EEPROM address configuration
#define EEPROM_SIZE 512

// SSID and SSID password's length should < 32 bytes
// http://serverfault.com/a/45509
#define SSID_LEN 32
#define PASS_LEN 32

#define CONNECTION_STRING_LEN 256
#define MESSAGE_MAX_LEN 256

#endif