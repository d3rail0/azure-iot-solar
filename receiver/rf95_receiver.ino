// Feather9x_RX
// -*- mode: C++ -*-
#include <SPI.h>
#include <RH_RF95.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "../lora/lora_solar_packets.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>

#include "config.h"

static bool messagePending = false;
static bool messageSending = true;

static char *connectionString;
static char *ssid;
static char *pass;

static int interval = INTERVAL;

/************************ Adafruit IO Config *******************************/
#define IO_USERNAME "d3rail0"
#define IO_KEY "<My IO key>"

/******************************* WIFI **************************************/
//   - Feather M0 WiFi -> https://www.adafruit.com/products/3010
#define WIFI_SSID "SKY13AF0"
#define WIFI_PASS "ME1003KF"
//
// feed name templates, all "0x00" parts will be replaced by I2C addresses like "0x45"
const char *current_template = "Ah_Sensor_0x00_current\0";
const char *voltage_template = "Ah_Sensor_0x00_voltage\0"; // idk, maybe Name should be working?

// #include <AdafruitIO_WiFi.h>
// #include <AdafruitIO_Feed.h>

// AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

/*for Feather32u4 RFM9x
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
#define VBATPIN A0
*/

Adafruit_SSD1306 display = Adafruit_SSD1306();

#define BUTTON_A 0
#define BUTTON_B 16
#define BUTTON_C 2
#define LED 0

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/*for feather m0 RFM9x
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
*/
// #if defined(ESP8266)
#define RFM95_CS 2   // "E"
#define RFM95_RST 16 // "D"
#define RFM95_INT 15 // "B"

/* for shield
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 7
*/

/* Feather 32u4 w/wing
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     2    // "SDA" (only SDA/SCL/RX/TX have IRQ!)
*/

/* Feather m0 w/wing
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     6    // "D"
*/

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 434.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Blinky on receipt
#define LED 13

// the device i2c address to use its voltage to display on OLED device
#define DISPLAY_DEVICE_I2C_ADDRESS 0x40

void init_wifi()
{
    // Attempt to connect to Wifi network:
    Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        // Get Mac Address and show it.
        // WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The huzzah board should
        // start from mac[0] to mac[5], but some other kinds of board run in the oppsite direction.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        Serial.printf("You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.\r\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ssid);
        WiFi.begin(ssid, pass);
        delay(10000);
    }
    Serial.printf("Connected to wifi %s.\r\n", ssid);
}

void init_time()
{
    time_t epochTime;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true)
    {
        epochTime = time(NULL);

        if (epochTime == 0)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
            break;
        }
    }
}

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
void setup()
{

    Serial.println("OLED Begun");
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();

    display.setTextSize(.5);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.display();

    pinMode(LED, OUTPUT);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    Serial.begin(115200);
    while (!Serial)
    {
        delay(1);
    }
    delay(100);

    Serial.println("Feather LoRa RX Test!");

    // manual reset
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    while (!rf95.init())
    {
        Serial.println("LoRa radio init failed");
        while (1);
    }
    Serial.println("LoRa radio init OK!");

    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
    if (!rf95.setFrequency(RF95_FREQ))
    {
        Serial.println("setFrequency failed");
        while (1);
    }
    Serial.print("Set Freq to: ");
    Serial.println(RF95_FREQ);

    // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

    // The default transmitter power is 13dBm, using PA_BOOST.
    // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
    // you can set transmitter powers from 5 to 23 dBm:
    rf95.setTxPower(23, false);

    init_serial();
    delay(2000);
    read_credentials();

    init_wifi();
    init_time();

    /*
     * AzureIotHub library remove AzureIoTHubClient class in 1.0.34, so we remove the code below to avoid
     *    compile error
     */

    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
    if (iotHubClientHandle == NULL)
    {
        Serial.println("Failed on IoTHubClient_CreateFromConnectionString.");
        while (1);
    }

    IoTHubClient_LL_SetOption(iotHubClientHandle, "product_info", "HappyPath_AdafruitFeatherHuzzah-C");
    IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
    IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
    IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, NULL);
}

static int messageCount = 1;

// This function will send a message to Azure and will wait until it has successfully sent the message before returning.
void send_to_Azure(transfer_packet *tp, int8_t *p_rssi)
{

    // SEND TO AZURE
    if (!messagePending && messageSending)
    {
        char messagePayload[MESSAGE_MAX_LEN];

        read_message(messageCount, messagePayload, tp, p_rssi);
        send_message(iotHubClientHandle, messagePayload);

        messageCount++;
        delay(INTERVAL);
    }

    IOTHUB_CLIENT_STATUS status;

    while ((IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY))
    {
        IoTHubClient_LL_DoWork(iotHubClientHandle);
        delay(100);
    }
    //=====================================================
}

void loop()
{

    abstract_packet *packet = receive_packet(rf95, 60000);
    if (!packet)
    {
        Serial.println("No packet, is there a sender around?");
        return;
    }
    
    int8_t packet_rssi = rf95.lastRssi();
    if (packet->get_type_id() != transfer_packet::TYPE_ID)
    {
        Serial.print("Ignoring received packet of type: 0x");
        Serial.println(packet->get_type_id());
        delete packet;
        return;
    }

    digitalWrite(LED, HIGH);

    float display_V = 0.0f;

    // upload to Adafruit IO
    transfer_packet *tp = (transfer_packet *)packet;
    // temperature_feed->save(tp->temperature_C);
    // packet_rssi_feed->save(packet_rssi);
    Serial.print("Num sensor_data packets: ");
    Serial.println(tp->num_packets);
    for (int i = 0; i < tp->num_packets; i++)
    {
        if (tp->packets[i]->get_type_id() != sensor_data_packet::TYPE_ID)
            continue;
        sensor_data_packet *sensor_data = (sensor_data_packet *)tp->packets[i];
        // find feed for corresponding sensor
        // Sensor_Feeds_List* feeds = get_feeds_for(sensor_data->i2c_addr);
        // Serial.print("feed_current->save "); Serial.println(sensor_data->current_A);
        // feeds->feed_current->save(sensor_data->current_A);
        // save data to display it later on OLED device
        if (sensor_data->i2c_addr == DISPLAY_DEVICE_I2C_ADDRESS)
        {
            display_V = sensor_data->bus_V;
        }
        //    Serial.print("feed_voltage->save "); Serial.println(sensor_data->current_A);
        // feeds->feed_voltage->save(sensor_data->bus_V);
    }

    send_to_Azure(tp, &packet_rssi);

    delete tp;

    // put data onto display
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("S_Voltage: ");
    display.print(display_V);
    display.println(" V");
    display.println(" ");
    display.print("RSSI: ");
    display.println(packet_rssi); // should be , DEC)
    delay(0);
    display.display();
    delay(100);
    display.clearDisplay();

    // send a reply
    reply_packet *reply = new reply_packet();
    reply->timestamp = millis();
    send_packet(reply, rf95);
    delete reply;

    digitalWrite(LED, LOW);
}
