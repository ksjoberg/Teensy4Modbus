#ifndef modbus_tcp_rtu_h
#define modbus_tcp_rtu_h

#include "HardwareSerial.h"
#include "lwip/tcp.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MODBUS_RTU_FRAME_SIZE   256
#define MODBUS_MAX_TCP_CONNECTIONS 4

class ModbusTcpRtu
{
private:
    void checkFrameReady(bool serialEvent = false);
    void processRtuFrame();
public:
	constexpr ModbusTcpRtu(
        HardwareSerial *serialDevice,
        uint8_t tx_enable_pin) :
        serialdev(serialDevice),
        tx_enable_pin_(tx_enable_pin)
    { }
	void configure(uint32_t ipaddress, uint16_t tcp_port, const uint8_t* rtu_slave_ids);
	void begin(uint32_t baud, uint16_t format=0);
	void end(void);
	void serialEvent(void);
	void doYield(void);
    
	void clientConnectionFailed(void);

	operator bool()			{ return true; }

    bool            outstanding_transaction = false;
    uint16_t        current_transaction = 0;
    struct tcp_pcb* active_pcb = nullptr;
	HardwareSerial * const serialdev;
private:
	uint8_t			tx_enable_pin_ = 0x0;
    uint32_t        last_serial_event = 0;
    uint32_t        frame_spacing = 0;
	ip4_addr		ip_address_ = { .addr = IPADDR_ANY };
	uint16_t		tcp_port_ = 0;
    const uint8_t  *rtu_slave_ids_ = nullptr;
    volatile uint8_t rtu_rx_buffer1[MODBUS_RTU_FRAME_SIZE] = { 0 };
    volatile uint8_t rtu_rx_buffer2[MODBUS_RTU_FRAME_SIZE] = { 0 };
    volatile uint8_t* rtu_rx_current = nullptr;
    size_t          rtu_rx_pos = 0;

    uint8_t *rtu_rx_frame = 0;
    size_t rtu_rx_frame_len = 0;
    struct tcp_pcb *main_pcb = 0;
    uint32_t        connection_failed_at = 0;

	void clientConnect(void);
};

#ifdef __cplusplus
}
#endif
#endif // modbus_tcp_rtu_h