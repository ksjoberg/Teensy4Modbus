#include "serialmenu.h"
#include "configuration.h"
#include "lwip/ip_addr.h"

#include <SerialCommands.h>

char serial_command_buffer_[80];
SerialCommands serial_commands_(&Serial, serial_command_buffer_, sizeof(serial_command_buffer_), " ");


void cmd_show(SerialCommands* sender)
{
    char buf[32];
    sender->GetSerial()->println("CURRENT CONFIGURATION");
    sender->GetSerial()->println("=====================");
    memcpy(&buf, &active_config.ipaddress, sizeof(active_config.ipaddress));
    sender->GetSerial()->printf("ip.address %d.%d.%d.%d\r\n", buf[0], buf[1], buf[2], buf[3]);
    memcpy(&buf, &active_config.netmask, sizeof(active_config.netmask));
    sender->GetSerial()->printf("ip.netmask %d.%d.%d.%d\r\n", buf[0], buf[1], buf[2], buf[3]);
    memcpy(&buf, &active_config.gateway, sizeof(active_config.gateway));
    sender->GetSerial()->printf("ip.gateway %d.%d.%d.%d\r\n", buf[0], buf[1], buf[2], buf[3]);
    sender->GetSerial()->printf("ip.dhcps ");
    if (active_config.dhcps_enabled) {
        sender->GetSerial()->println("on");
    } else {
        sender->GetSerial()->println("off");
    }

    sender->GetSerial()->println("# TCP <-> RTU interfaces:");
    for(int i=0; i<MODBUS_GW_COUNT; i++)
    {
        if (active_config.modbus_gw[i].ipaddress != 0)
        {
            sender->GetSerial()->printf("  # Mode TCP client (RTU slave), use \"set if %d ip.address 0.0.0.0\" to switch mode\r\n", i);
            sender->GetSerial()->println("  # Connects to ip/port:");
            memcpy(&buf, &active_config.modbus_gw[i].ipaddress, sizeof(active_config.modbus_gw[i].ipaddress));
            sender->GetSerial()->printf("  if %d ip.address %d.%d.%d.%d\r\n", i, buf[0], buf[1], buf[2], buf[3]);
        } else {
            sender->GetSerial()->printf("  # Mode TCP server (RTU master), use \"set if %d ip.address <address>\" to switch mode\r\n", i);
            sender->GetSerial()->println("  # Listens on port:");
        }

        sender->GetSerial()->printf("  if %d ip.port %d\r\n", i, active_config.modbus_gw[i].tcp_port);
        sender->GetSerial()->printf("  if %d baudrate %d\r\n", i, active_config.modbus_gw[i].baudrate);
        if (active_config.modbus_gw[i].ipaddress != 0)
        {
            sender->GetSerial()->printf("  if %d rtu.unitids", i);
            uint8_t *p = active_config.modbus_gw[i].unit_filter;
            if (*p == 0)
            {
                sender->GetSerial()->println(" 0");
            } else {
                do
                {
                    sender->GetSerial()->printf(" %d", *p);
                } while(*(++p));
                sender->GetSerial()->println();
            }
        }
        sender->GetSerial()->println();
    }
    sender->GetSerial()->println();
}

#define STR2(a) #a
#define STR(a) STR2(a)

void cmd_set_if(SerialCommands* sender, uint8_t index)
{
    if (index >= MODBUS_GW_COUNT)
    {
        sender->GetSerial()->printf("Expected index between 0 and "  STR(MODBUS_GW_COUNT-1)  "\r\n");
        return;
    }
    char* key = sender->Next();
    if (!key)
    {
        sender->GetSerial()->println("Expected two additional arguments (missing key and value): <key> <value>");
        return;
    }
    char* value = sender->Next();
    if (!value)
    {
        sender->GetSerial()->println("Expected two additional arguments (missing value): <key> <value>");
        return;
    }

    if (strcmp(key, "ip.address") == 0)
    {
        active_config.modbus_gw[index].ipaddress = ipaddr_addr(value);
    } else if (strcmp(key, "ip.port") == 0)
    {
        active_config.modbus_gw[index].tcp_port = atoi(value);
    } else if (strcmp(key, "ip.baudrate") == 0)
    {
        active_config.modbus_gw[index].baudrate = atoi(value);
    } else if (strcmp(key, "rtu.unitids") == 0) {
        uint8_t *p = active_config.modbus_gw[index].unit_filter;
        uint8_t *pend = p + sizeof(active_config.modbus_gw[index].unit_filter);
        do {
            *p = atoi(value);
            value = sender->Next();
        } while (p < (pend-1) && *p++ != 0 && value != nullptr);
        *p = 0;
    } else {
        sender->GetSerial()->printf("Unknown interface setting '%s'\r\n", key);
    }
}

void cmd_set(SerialCommands* sender)
{
    char* key = sender->Next();
    if (!key)
    {
        sender->GetSerial()->println("Expected two arguments (missing key and value): <key> <value>");
        return;
    }
    char* value = sender->Next();
    if (!value)
    {
        sender->GetSerial()->println("Expected two arguments (missing value): <key> <value>");
        return;
    }
    if (strcmp(key, "if") == 0)
    {
        int idx = atoi(value);
        cmd_set_if(sender, idx);

    } else if (strcmp(key, "ip.address") == 0)
    {
        active_config.ipaddress = ipaddr_addr(value);
    } else if (strcmp(key, "ip.netmask") == 0)
    {
        active_config.netmask = ipaddr_addr(value);
    } else if (strcmp(key, "ip.gateway") == 0)
    {
        active_config.gateway = ipaddr_addr(value);
    }
}


void cmd_save(SerialCommands* sender)
{
    configuration_save();
    sender->GetSerial()->println("Configuration has been saved. Reset the device to activate.");
}

void cmd_reset(SerialCommands* sender)
{
    configuration_reset();
    configuration_save();
    sender->GetSerial()->println("Configuration has been reset. Reset the device to activate.");
}

SerialCommand cmd_reset_("reset", cmd_reset);
SerialCommand cmd_set_("set", cmd_set);
SerialCommand cmd_save_("save", cmd_save);
SerialCommand cmd_show_("show", cmd_show);


//This is the default handler, and gets called when no other command matches. 
void cmd_unrecognized(SerialCommands* sender, const char* cmd)
{
  sender->GetSerial()->print("Unrecognized command [");
  sender->GetSerial()->print(cmd);
  sender->GetSerial()->println("]");
  sender->GetSerial()->println("Available commands:");
  sender->GetSerial()->println(" * 'reset' - Factory resets the configuration");
  sender->GetSerial()->println(" * 'show' - Shows the (currently unsaved?) configuration");
  sender->GetSerial()->println(" * 'save' - Saves the configuration shown by 'show'");
  sender->GetSerial()->println(" * 'set key value' - Sets the configuration item key to value");
  sender->GetSerial()->println(" * 'set if X key value' - For interface X, sets the configuration item key to value");
  sender->GetSerial()->println();
}

void MenuSetup() {
  serial_commands_.SetDefaultHandler(cmd_unrecognized);
  serial_commands_.AddCommand(&cmd_reset_);
  serial_commands_.AddCommand(&cmd_set_);
  serial_commands_.AddCommand(&cmd_save_);
  serial_commands_.AddCommand(&cmd_show_);
}

void MenuPoll() {
  SERIAL_COMMANDS_ERRORS t = serial_commands_.ReadSerial();
}
