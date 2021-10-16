#include "configuration.h"
#include <EEPROM.h>
#include <string.h>
#include "debug/printf.h"

const configuration_t default_config = {
    .magic = 0xf00dface,
    .ipaddress = lwip_htonl(LWIP_MAKEU32(192, 168, 7, 1)),
    .netmask = lwip_htonl(LWIP_MAKEU32(255, 255, 255, 0)),
    .gateway = lwip_htonl(LWIP_MAKEU32(0, 0, 0, 0)),
    .dhcps_enabled = true,
    .modbus_gw = {
        {
            .ipaddress = lwip_htonl(LWIP_MAKEU32(0, 0, 0, 0)),
            .tcp_port = 502,
            .baudrate = 9600,
            .unit_filter = { 0 }
        },
        {
            .ipaddress = lwip_htonl(LWIP_MAKEU32(0, 0, 0, 0)),
            .tcp_port = 503,
            .baudrate = 9600,
            .unit_filter = { 0 }
        },
        {
            .ipaddress = lwip_htonl(LWIP_MAKEU32(0, 0, 0, 0)),
            .tcp_port = 504,
            .baudrate = 9600,
            .unit_filter = { 0 }
        }
    }
};

configuration_t active_config;
#define CONFIG_EEPROM_OFFSET 0
void configuration_load()
{
    uint8_t* dest = (uint8_t*)&active_config;
    for(size_t i=0; i<sizeof(configuration_t); i++)
    {
        *dest++ = EEPROM.read(CONFIG_EEPROM_OFFSET + i);
    }
    if (active_config.magic != default_config.magic)
    {
        printf("Invalid magic, resetting config: %x != %x\n", active_config.magic, default_config.magic);
        configuration_reset();
    }
}

void configuration_reset()
{
    memcpy(&active_config, &default_config, sizeof(configuration_t));
}

void configuration_save()
{
    uint8_t* dest = (uint8_t*)&active_config;
    for(size_t i=0; i<sizeof(configuration_t); i++)
    {
        uint8_t current = EEPROM.read(CONFIG_EEPROM_OFFSET + i);
        if (current != *dest)
        {
            EEPROM.write(CONFIG_EEPROM_OFFSET + i, *dest);
        }
        dest++;
    }
}