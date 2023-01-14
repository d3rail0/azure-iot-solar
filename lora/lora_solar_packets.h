// lora_solar_packets.h
// -*- mode: C++ -*-
#ifndef LORA_SOLAR_PACKETS_H
#define LORA_SOLAR_PACKETS_H

#include "Arduino.h"
#include <RH_RF95.h>

void put_uint8(uint8_t value, uint8_t **dest);
void put_uint16(uint16_t value, uint8_t **dest);
void put_uint32(uint32_t value, uint8_t **dest);
void put_float(float value, uint8_t **dest);

uint8_t  get_uint8(uint8_t **source);
uint16_t get_uint16(uint8_t **source);
uint32_t get_uint32(uint8_t **source);
float    get_float(uint8_t **source);

struct abstract_packet
{
    virtual void write_big_endian(uint8_t **ref) = 0;
    virtual void read_big_endian(uint8_t **ref) = 0;
    virtual uint8_t get_type_id() = 0;
    virtual uint16_t data_size() = 0;
};

uint16_t packet_size(abstract_packet *packet);

abstract_packet *create_packet_from_id(uint8_t packet_type);
abstract_packet *read_packet(uint8_t **ref);
void write_packet(abstract_packet *packet, uint8_t **ref);

void send_packet(abstract_packet *packet, RH_RF95 &rf95);
abstract_packet *receive_packet(RH_RF95 &rf95, uint16_t timeout);
void debug_data(uint8_t *packet_data, uint16_t packet_size);

struct sensor_data_packet : public abstract_packet
{
    static const uint8_t TYPE_ID = 1;

    uint8_t i2c_addr;
    float shunt_V;
    float bus_V;
    float current_A;

    void debug_packet()
    {
        float load_V = bus_V + shunt_V;
        Serial.print("Sensor 0x");
        Serial.print(i2c_addr, 16);
        Serial.print(": bus ");
        Serial.print(bus_V);
        Serial.print(" V, shunt ");
        Serial.print(shunt_V);
        Serial.print(" V, load  ");
        Serial.print(load_V);
        Serial.print(" V, current ");
        Serial.print(current_A);
        Serial.println(" A");
    }

    virtual void write_big_endian(uint8_t **ref)
    {
        put_uint8(i2c_addr, ref);
        put_float(shunt_V, ref);
        put_float(bus_V, ref);
        put_float(current_A, ref);
    }

    virtual void read_big_endian(uint8_t **ref)
    {
        i2c_addr  = get_uint8(ref);
        shunt_V   = get_float(ref);
        bus_V     = get_float(ref);
        current_A = get_float(ref);
    }

    virtual uint8_t get_type_id()
    {
        return TYPE_ID;
    }

    virtual uint16_t data_size()
    {
        return sizeof(uint8_t) + sizeof(float) * 3;
    }
};

struct transfer_packet : public abstract_packet
{
    static const uint8_t TYPE_ID = 2;

    uint32_t timestamp;
    uint16_t packet_counter;
    float temperature_C;
    float current_INA169;

    float longtitude;
    float latitude;

    uint16_t num_packets      = 0;
    abstract_packet **packets = 0;

    virtual void write_big_endian(uint8_t **ref)
    {
        put_uint32(this->timestamp, ref);
        put_uint16(this->packet_counter, ref);
        put_float(this->temperature_C, ref);
        put_float(this->current_INA169, ref);
        put_float(this->longtitude, ref);
        put_float(this->latitude, ref);
        put_uint16(this->num_packets, ref);
        for (int i = 0; i < this->num_packets; i++)
            write_packet(packets[i], ref);
    }

    virtual void read_big_endian(uint8_t **ref)
    {
        this->timestamp      = get_uint32(ref);
        this->packet_counter = get_uint16(ref);
        this->temperature_C  = get_float(ref);
        this->current_INA169 = get_float(ref);
        this->longtitude     = get_float(ref);
        this->latitude       = get_float(ref);
        this->num_packets    = get_uint16(ref);
        this->packets        = new abstract_packet *[this->num_packets];
        for (int i = 0; i < this->num_packets; i++)
        {
            this->packets[i] = read_packet(ref);
        }
    }

    virtual uint8_t get_type_id()
    {
        return TYPE_ID;
    }

    virtual uint16_t data_size()
    {
        uint16_t result = sizeof(uint32_t) + sizeof(float) * 4 + sizeof(uint16_t) * 2;
        for (int i = 0; i < num_packets; i++)
            result += packet_size(packets[i]);
        return result;
    }

    ~transfer_packet()
    {
        if (packets != 0)
        {
            for (int i = 0; i < num_packets; i++)
                if (packets[i] != 0)
                    delete packets[i];
            delete[] packets;
        }
    }
};

struct reply_packet : public abstract_packet
{
    static const uint8_t TYPE_ID = 3;

    uint32_t timestamp;
    uint16_t packet_counter;

    void debug_packet()
    {
        Serial.print("Reply timestamp: ");
        Serial.print(timestamp);
        Serial.println(" ms");
    }

    virtual void write_big_endian(uint8_t **ref)
    {
        put_uint32(timestamp, ref);
        put_uint16(packet_counter, ref);
    }

    virtual void read_big_endian(uint8_t **ref)
    {
        timestamp      = get_uint32(ref);
        packet_counter = get_uint16(ref);
    }

    virtual uint8_t get_type_id()
    {
        return TYPE_ID;
    }

    virtual uint16_t data_size()
    {
        return sizeof(uint32_t) + sizeof(uint16_t);
    }
};

#endif
