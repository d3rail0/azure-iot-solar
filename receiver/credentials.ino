#include "utils.h"

// Read parameters from EEPROM or Serial
void read_credentials()
{
    int ssidAddr             = 0;
    int passAddr             = ssidAddr + SSID_LEN;
    int connectionStringAddr = passAddr + SSID_LEN;

    // malloc for parameters
    ssid = (char *)malloc(SSID_LEN);
    pass = (char *)malloc(PASS_LEN);
    connectionString = (char *)malloc(CONNECTION_STRING_LEN);

    // try to read out the credential information, if failed, the length should be 0.
    int ssidLength = EEPROM_read(ssidAddr, ssid);
    int passLength = EEPROM_read(passAddr, pass);
    int connectionStringLength = EEPROM_read(connectionStringAddr, connectionString);

    Serial.print("CONNECTION STRING: "); Serial.println(connectionString);
    Serial.print("SSID: ");              Serial.println(ssid);
    Serial.print("PWD: ");               Serial.println(pass);

    if (ssidLength > 0 && passLength > 0 && connectionStringLength > 0 && !need_erase_EEPROM())
        return;
    
    // read from Serial and save to EEPROM
    read_from_serial("Input your Wi-Fi SSID: ", ssid, SSID_LEN, 0);
    EEPROM_write(ssidAddr, ssid, strlen(ssid));

    read_from_serial("Input your Wi-Fi password: ", pass, PASS_LEN, 0);
    EEPROM_write(passAddr, pass, strlen(pass));

    read_from_serial("Input your Azure IoT hub device connection string: ", connectionString, CONNECTION_STRING_LEN, 0);
    EEPROM_write(connectionStringAddr, connectionString, strlen(connectionString));
}
