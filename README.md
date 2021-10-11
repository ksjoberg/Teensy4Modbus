# Teensy4Modbus

This application uses the CDC-ECM mode implemented in the Teensy Cores fork at https://github.com/ksjoberg/cores/tree/teensy4-ecm to expose the Teensy as a network device to a computer.

The vision is to add a ModbusTCP listener so that the Teensy can act as a ModbusTCP to ModbusRTU gateway using one or more of its seven hardware serial ports.

The inspiration sprung out of the fact that PyModbus can't properly participate on a Modbus serial bus with the RTU protocol: the framing information is lost as the serial port exposes the bus as a stream of bytes. The RTU frames are defined as being preceeded by a 3.5 char time silence on the bus, but the usermode applications usually just calls the serial port read function to to fill a provided buffer. This read call bridges the inter-frame bus silence and may returns multiple partial frames in one read.

ModbusTCP handles the framing correctly, so if PyModbus can talk to a ModbusTCP device instead it will work as intended.
