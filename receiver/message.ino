#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>

bool read_message(int messageId, char *payload, transfer_packet *tp, int8_t *p_rssi)
{

    // StaticJsonBuffer<MESSAGE_MAX_LEN> jsonBuffer;
    DynamicJsonBuffer jsonBuffer(MESSAGE_MAX_LEN);

    JsonObject &root = jsonBuffer.createObject();
    root["dId"] = DEVICE_ID;
    root["mId"] = messageId;

    if (p_rssi != NULL)
        root["RSSI"] = *p_rssi;
    else
        root["RSSI"] = NULL;

    if (std::isnan(tp->temperature_C))
        root["T"] = NULL;
    else
        root["T"] = tp->temperature_C;

    if (std::isnan(tp->current_INA169))
    {
        root["LED"] = NULL;
        root["Sts"] = "Off";
    }
    else
    {
        root["LED"] = tp->current_INA169;
        root["Sts"] = (tp->current_INA169 > 0.1f) ? "On" : "Off";
    }

    if (std::isnan(tp->longtitude))
        root["long"] = NULL;
    else
        root["long"] = tp->longtitude;

    if (std::isnan(tp->latitude))
        root["lat"] = NULL;
    else
        root["lat"] = tp->latitude;

    String pName = "";
    for (int i = 0; i < tp->num_packets; i++)
    {
        if (tp->packets[i]->get_type_id() != sensor_data_packet::TYPE_ID)
            continue;
        sensor_data_packet *sensor_data = (sensor_data_packet *)tp->packets[i];
        switch (i)
        {
        case 0:
            pName = "S_";
            break;
        case 1:
            pName = "U_";
            break;
        case 2:
            pName = "B_";
            break;
        }

        if (std::isnan(sensor_data->bus_V))
            root[pName + "BV"] = NULL;
        else
            root[pName + "BV"] = sensor_data->bus_V;

        if (std::isnan(sensor_data->current_A))
            root[pName + "CA"] = NULL;
        else
            root[pName + "CA"] = sensor_data->current_A;

        if (std::isnan(sensor_data->bus_V) || std::isnan(sensor_data->current_A))
            root[pName + "Load"] = NULL;
        else
            root[pName + "Load"] = sensor_data->bus_V + sensor_data->shunt_V;
    }

    Serial.print("JSON object size: ");
    Serial.println(root.size());
    Serial.print("JSON buffer size: ");
    Serial.println(jsonBuffer.size());

    root.printTo(payload, MESSAGE_MAX_LEN);
    return true;
}

void parse_twin_message(char *message)
{
    StaticJsonBuffer<MESSAGE_MAX_LEN> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(message);
    if (!root.success())
    {
        Serial.printf("Parse %s failed.\r\n", message);
        return;
    }

    if (root["desired"]["interval"].success())
    {
        interval = root["desired"]["interval"];
    }
    else if (root.containsKey("interval"))
    {
        interval = root["interval"];
    }
}
