// -*- mode: C++ -*-
#include "lora_solar_packets.h"

uint16_t packet_size(abstract_packet *packet)
{
    return sizeof(uint8_t) + (packet != 0 ? packet->data_size() : 0);
}

/*
 * Sends given packet over given RH_RF95.
 */
void send_packet(abstract_packet *packet, RH_RF95 &rf95)
{
    // calculate total size of packet
    uint16_t buf_size      = packet_size(packet);

    // allocate buffer from heap
    uint8_t *send_buffer   = new uint8_t[buf_size];

    // serialize big-endian values into buffer
    uint8_t *buffer_offset = send_buffer;

    write_packet(packet, &buffer_offset);

    // send data over RF95s
    rf95.send(send_buffer, buf_size);
    rf95.waitPacketSent();
    
    // deallocate buffer from heap
    delete send_buffer;
}

/*
 * Receives given packet from given RH_RF95.
 */
abstract_packet *receive_packet(RH_RF95 &rf95, uint16_t timeout)
{
    // check if we have a received message
    if (!rf95.waitAvailableTimeout(timeout))
    {
        // no packet around
        return 0;
    }

    // allocate buffer
    uint8_t packet_data[RH_RF95_MAX_PAYLOAD_LEN];
    uint8_t actual_len = RH_RF95_MAX_PAYLOAD_LEN;
    if (!rf95.recv(packet_data, &actual_len))
    {
        return 0;
    }
    //  debug_data(packet_data, len);
    uint8_t *buf_offset = packet_data;
    return read_packet(&buf_offset);
}

/*
 * Prints hex data into serial port.
 */
void debug_data(uint8_t *packet_data, uint16_t packet_size)
{
    while (packet_size-- > 0)
    {
        uint8_t b = *packet_data++;
        Serial.print(b >> 4, 16);
        Serial.print(b & 0xf, 16);
        Serial.print(" ");
    }
    Serial.println();
}

/*
 * Creates packet based on given packet_type.
 */
abstract_packet *create_packet_from_id(uint8_t packet_type)
{
    switch (packet_type)
    {
    case sensor_data_packet::TYPE_ID:
        return new sensor_data_packet();
    case transfer_packet::TYPE_ID:
        return new transfer_packet();
    case reply_packet::TYPE_ID:
        return new reply_packet();
    default:
        return 0;
    }
}

abstract_packet *read_packet(uint8_t **ref)
{
    uint8_t type_id = get_uint8(ref);
    if (type_id == 0)
        return 0;
    abstract_packet *packet = create_packet_from_id(type_id);
    if (packet != 0)
        packet->read_big_endian(ref);
    return packet;
}

void write_packet(abstract_packet *packet, uint8_t **ref)
{
    if (packet == 0)
    {
        put_uint8(0, ref);
    }
    else
    {
        put_uint8(packet->get_type_id(), ref);
        packet->write_big_endian(ref);
    }
}

// === big endian writing functions ===

void put_uint8(uint8_t value, uint8_t **dest)
{
    *(*dest)++ = (uint8_t)(value & 0xff);
}

void put_uint16(uint16_t value, uint8_t **dest)
{
    *(*dest)++ = (uint8_t)((value >> 8) & 0xff);
    *(*dest)++ = (uint8_t)(value & 0xff);
}

void put_uint32(uint32_t value, uint8_t **dest)
{
    *(*dest)++ = (uint8_t)((value >> 24) & 0xff);
    *(*dest)++ = (uint8_t)((value >> 16) & 0xff);
    *(*dest)++ = (uint8_t)((value >> 8)  & 0xff);
    *(*dest)++ = (uint8_t)(value & 0xff);
}

void put_float(float value, uint8_t **dest)
{
    uint32_t value_int = *((uint32_t *)&value);
    _Static_assert(sizeof(value) == sizeof(value_int),
                   "'float' and 'uint32_t' types have different sizes, change type of 'value_int' variable to uintXX_t to match float size!");
    put_uint32(value_int, dest);
}

// === big endian reading functions ===

uint8_t get_uint8(uint8_t **source)
{
    return (uint8_t) * (*source)++;
}

uint16_t get_uint16(uint8_t **source)
{
    return ((uint16_t) * (*source)++) << 8 |
           (uint16_t) * (*source)++;
}

uint32_t get_uint32(uint8_t **source)
{
    return ((uint32_t) * (*source)++) << 24 |
           ((uint32_t) * (*source)++) << 16 |
           ((uint32_t) * (*source)++) << 8 |
           (uint32_t) * (*source)++;
}

float get_float(uint8_t **source)
{
    uint32_t value_int = get_uint32(source);
    float value = *((float *)&value_int);
    _Static_assert(sizeof(value) == sizeof(value_int),
                   "'float' and 'uint32_t' types have different sizes, change type of 'value_int' variable to uintXX_t to match float size!");
    return value;
}
