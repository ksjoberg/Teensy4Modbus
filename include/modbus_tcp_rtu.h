#ifndef modbus_tcp_rtu_h
#define modbus_tcp_rtu_h

#include "HardwareSerial.h"
#include "lwip/tcp.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MODBUS_RTU_FRAME_SIZE   256
#define MODBUS_MAX_TCP_CONNECTIONS 4

enum MODBUSTCPRTU_FLAGS {
    DEBUG_RTU   = 0x01,
    DEBUG_TCP   = 0x02
};

struct modbus_gw_counters {
    uint32_t rtu_rx_frames_dropped_badlen;
    uint32_t rtu_rx_frames_dropped_badcrc;
    uint32_t rtu_rx_frames_dropped_notforme;
    uint32_t rtu_rx_frames_dropped_notcp;
    uint32_t rtu_rx_frames;
    uint32_t rtu_tx_frames;
    uint32_t rtu_rx_bytes;
    uint32_t rtu_tx_bytes;
    uint32_t tcp_rx_frames;
    uint32_t tcp_tx_frames;
    uint32_t tcp_rx_bytes;
    uint32_t tcp_tx_bytes;
};

class ModbusTcpRtu
{
private:
    void checkFrameReady(bool serialEvent = false);
    void processRtuFrame();
public:
	constexpr ModbusTcpRtu(
        HardwareSerial *serial_device,
        uint8_t tx_enable_pin,
        const char* name,
        Stream *debug_stream) :
        serialdev(serial_device),
        name(name),
        debug_stream(debug_stream),
        tx_enable_pin_(tx_enable_pin)
    { }
	void configure(uint32_t ipaddress, uint16_t tcp_port, const uint8_t* rtu_slave_ids);
    void setFlags(uint8_t flags);
	void begin(uint32_t baud, uint16_t format=0);
	void end(void);
	void serialEvent(void);
	void doYield(void);
    
	void clientConnectionFailed(void);

	operator bool()			{ return true; }

    bool            outstanding_transaction = false;
    uint16_t        current_transaction = 0;
    struct tcp_pcb* active_pcb = nullptr;
    struct modbus_gw_counters counters = { 0 };
	HardwareSerial * const serialdev;
    const char*     name = nullptr;
    Stream*         debug_stream;
    uint8_t         debug_flags = 0;
private:
	uint8_t			tx_enable_pin_ = 0x0;
    uint32_t        last_serial_event = 0;
    uint32_t        frame_spacing = 0;
	ip4_addr		ip_address_ = { .addr = IPADDR_ANY };
	uint16_t		tcp_port_ = 0;
    const uint8_t*  rtu_slave_ids_ = nullptr;
    volatile uint8_t rtu_rx_buffer1[MODBUS_RTU_FRAME_SIZE] = { 0 };
    volatile uint8_t rtu_rx_buffer2[MODBUS_RTU_FRAME_SIZE] = { 0 };
    volatile uint8_t* rtu_rx_current = nullptr;
    size_t          rtu_rx_pos = 0;

    uint8_t *rtu_rx_frame = 0;
    size_t rtu_rx_frame_len = 0;
    struct tcp_pcb *main_pcb = 0;
    uint32_t        connection_failed_at = 0;

	void clientConnect(void);
    friend err_t server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
};

#ifdef __cplusplus
}
#endif
#endif // modbus_tcp_rtu_h