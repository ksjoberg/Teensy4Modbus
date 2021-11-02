#ifndef configuration_h
#define configuration_h
#include <stdint.h>
#include <stdbool.h>
#include "lwip/def.h"

#define MODBUS_GW_COUNT 3

typedef struct cfg_modbus_gw {
    uint32_t ipaddress; // if set, behave as a TCP client and connect to here (implies RTU slave)...
    uint16_t tcp_port; // ...on this port. Otherwise, listen on this port and act as master on the RTU side.
    uint32_t baudrate;
    uint8_t  unit_filter[31];
    uint8_t  flags;
} cfg_modbus_gw_t;

typedef struct configuration {
    uint32_t magic;
    uint32_t ipaddress;
    uint32_t netmask;
    uint32_t gateway;
    bool dhcps_enabled:1;
    cfg_modbus_gw_t modbus_gw[MODBUS_GW_COUNT];
} configuration_t;

extern configuration_t active_config;

void configuration_load();
void configuration_reset();
void configuration_save();

#endif //configuration_h
