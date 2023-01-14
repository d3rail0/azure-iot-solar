// Feather9x_TX
// -*- mode: C++ -*-
#include <Wire.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <Adafruit_INA219.h>
#include "../lora/lora_solar_packets.h"

/**
 * This class is created to keep i2c address public within Sensor object.
 */
class Sensor_INA219 : public Adafruit_INA219
{
public:
	uint8_t i2c_addr;

	Sensor_INA219(uint8_t i2c_addr) : Adafruit_INA219(i2c_addr)
	{
		this->i2c_addr = i2c_addr;
	}
};

#define lenof(T) (sizeof(T) / sizeof(*(T)))
#define TEMP_PIN A0

/* for feather32u4
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
*/

// for feather m0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

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

#if defined(ESP8266)
/* for ESP w/featherwing */
#define RFM95_CS 2	 // "E"
#define RFM95_RST 16 // "D"
#define RFM95_INT 15 // "B"

#elif defined(ESP32)
/* ESP32 feather w/wing */
#define RFM95_RST 27 // "A"
#define RFM95_CS 33	 // "B"
#define RFM95_INT 12 //  next to A

#elif defined(NRF52)
/* nRF52832 feather w/wing */
#define RFM95_RST 7	 // "A"
#define RFM95_CS 11	 // "B"
#define RFM95_INT 31 // "C"

#elif defined(TEENSYDUINO)
/* Teensy 3.x w/wing */
#define RFM95_RST 9 // "A"
#define RFM95_CS 10 // "B"
#define RFM95_INT 4 // "C"
#endif

// Change to 434.0 or other frequency, must match RX's freq
#define RF95_FREQ 434.0

// Period of time between two packets send, in seconds
#define PACKET_SEND_PERIOD_SECONDS 30

// How long should the program wait for reply packets
// before timing out (in seconds).
#define PACKET_RECEIVE_TIMEOUT_PERIOD 2

// What pin to connect the sensor to
#define THERMISTORPIN A0

// INA 169 SENSOR
#define INA169_PIN A1

// UNCOMMENT BELOW ONLY IF YOU WANT TO CHANGE VALUE OF RS or VOLTAGE_REF
// AND THEN UNCOMMENT OTHER part IN read_ina169() function as well, but comment first part
// const int RS = 1;
// const int VOLTAGE_REF = 1;

Sensor_INA219 *ina219_sensors[] = {
	new Sensor_INA219(0x40), // Allocated to Solar   - No Jumper
	new Sensor_INA219(0x44), // Allocated to LED     - A1 Jumper
	new Sensor_INA219(0x41)	 // Allocated to Battery - A0 Jumper
};

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// thermistor reading code
// to increase precision from +-1°C to near +-0.05°C, add error correcting polynomial:
const float err_correct_poly[] = {
	-5.18855e-06, 8.48081e-05, -0.000326449, -0.00139878,
	0.011278    , -0.00456384, -0.101367   , 0.145696,
	0.341763    , -0.625131  , -0.612054   , 0.753828,
	0.635072
};

// calculates polynomial from given X value and coefficient
float calc_poly(float x, const float *poly, int num)
{
	float power = 1, summ = 0;
	for (int i = num; i > 0; summ += power * poly[--i], power *= x) {}
	return summ;
}


// longtitude and latitude of this device
const float longtitude = -0.54411;
const float latitude   = 51.384506;

void setup()
{
	pinMode(RFM95_RST, OUTPUT);
	digitalWrite(RFM95_RST, HIGH);

	// initialize serial
	Serial.begin(115200);
	while (!Serial)
	{
		delay(1);
	}
	delay(100);

	// initialize all sensors
	for (int i = 0; i < lenof(ina219_sensors); i++)
		ina219_sensors[i]->begin();

	Serial.println("Feather LoRa TX Test!");

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
	pinMode(THERMISTORPIN, INPUT);

	Serial.println("Measuring voltage and current with INA219 ...");
}

sensor_data_packet *read_from_sensor(Sensor_INA219 *sensor)
{
	sensor_data_packet *result = new sensor_data_packet();

	result->i2c_addr  = sensor->i2c_addr;
	result->shunt_V   = sensor->getShuntVoltage_mV() / 1000.0f;
	result->bus_V     = sensor->getBusVoltage_V();
	result->current_A = sensor->getCurrent_mA() / 1000.0f;
	return result;
}

float read_thermistor()
{
	analogReference(AR_INTERNAL1V0);
	float V1  = analogRead(TEMP_PIN) * 1.0 /* volts */ / 1023.0 /* analogRead's 0..1023 integer value */;
	float Vcc = 3.3;						  // set this to whatever you feed to resistor&thermistor in series, preferably use stable voltage
	float R2  = 195000.0;					  // resistor in series with thermistor, in Ohms
	float R1  = V1 * R2 / (Vcc - V1) / 1000.0; // in kOhms

	// this formula is generated by manual Python script, it is approximation of data provided in:
	// https://cdn-shop.adafruit.com/datasheets/103_3950_lookuptable.pdf
	// approximation error is +-1 degrees of C
	float degrees_C = (1 / pow(R1 - 0.01205333566976, 0.1213)) * 257.1641435843359 - 169.98758857779046;

	// following polynomial is generated to correct approximation error
	// it reduces difference from datasheet data points to +-0.05 degrees of C typical, +-0.1 degrees of C max
	float polynomial_error_correction = calc_poly(log(R1), err_correct_poly, lenof(err_correct_poly));

	return degrees_C - polynomial_error_correction;
}

float sensorValue; // Variable to store value from analog read
float current;	   // Calculated current value

float read_ina169()
{

	// IF VOLTAGE REFERENCE AND RS ARE COMMENTED USE THIS PART: THIS IS FASTER
	sensorValue = analogRead(INA169_PIN);
	sensorValue = (sensorValue) / 1023;
	return sensorValue;

	// IF VOLTAGE REFERENCE AND RS ARE UNCOMMENTED USE THIS PART:
	/*
	sensorValue = analogRead(INA169_PIN);
	sensorValue = (sensorValue * VOLTAGE_REF) / 1023;
	current = sensorValue / (1 * RS);
	return current;
	*/
}

// packet counter, we increment per xmission
uint32_t global_packetnum = 0;

void loop()
{

	// collect info
	int num_sensors = lenof(ina219_sensors);
	Serial.print("Reading data from ");
	Serial.print(num_sensors);
	Serial.println(" sensors...");

	transfer_packet *packet = new transfer_packet();
	packet->timestamp       = millis();
	packet->packet_counter  = ++global_packetnum;
	packet->temperature_C   = read_thermistor();
	packet->current_INA169  = read_ina169();
	packet->longtitude      = longtitude;
	packet->latitude        = latitude;

	Serial.print("Temperature reading: ");
	Serial.print(packet->temperature_C);
	Serial.println(" degrees of C");
	Serial.print("INA 169 Current: ");
	Serial.print(packet->current_INA169);
	Serial.println(" A");
	Serial.print("LONGTITUDE: ");
	Serial.println(packet->longtitude);
	Serial.print("LATITUDE: ");
	Serial.println(packet->latitude);

	// Read sensor data from each sensor 
	packet->num_packets = num_sensors;
	packet->packets = new abstract_packet *[num_sensors];
	for (int i = 0; i < num_sensors; i++)
	{
		sensor_data_packet *data = read_from_sensor(ina219_sensors[i]);
		packet->packets[i] = data;
		data->debug_packet();
	}

	sensor_data_packet *solar_packet   = (sensor_data_packet *)packet->packets[0];
	sensor_data_packet *battery_packet = (sensor_data_packet *)packet->packets[2];

	// SOLAR BUS_V = (BATTERY BUS V) - (SOLAR BUS V)
	solar_packet->bus_V = battery_packet->bus_V - solar_packet->bus_V;
	packet->packets[0]  = solar_packet;

	//======================

	// send a packet
	send_packet(packet, rf95);
	Serial.print("Sent ");
	Serial.print(packet_size(packet));
	Serial.println(" bytes packet");
	delete packet;

	// receive replies as long as we get them
	abstract_packet *reply;
	while (reply = receive_packet(rf95, PACKET_RECEIVE_TIMEOUT_PERIOD * 1000))
	{
		if (reply->get_type_id() == reply_packet::TYPE_ID)
		{
			Serial.print("Reply of ");
			Serial.print(packet_size(reply));
			Serial.print(" bytes, RSSI: ");
			Serial.print(rf95.lastRssi(), DEC);
			Serial.print(", ");
			((reply_packet *)reply)->debug_packet();
		}
		delete reply;
	}
	
	delay(PACKET_SEND_PERIOD_SECONDS * 1000); // wait before another transmit!
}