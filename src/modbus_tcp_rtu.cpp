#include "modbus_tcp_rtu.h"
#include "debug/printf.h"

#define TCP_RECONNECTION_INTERVAL   5000

#ifndef __ARMEB__   /* Little endian */
typedef union
{
  uint16_t u16;
  uint8_t  u8 [2];
} be16_t;

#define BIGENDIAN16(x) ((be16_t){ .u8 = { \
    (uint8_t)((x) >> 8), \
    (uint8_t)(x) } }.u16)

#else /* Big endian */
#define BIGENDIAN16(x) (x)
#define BIGENDIAN32(x) (x)
#endif

const uint16_t crc16_table[] = {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
    0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
    0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
    0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
    0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
    0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
    0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
    0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
    0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
    0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
    0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
    0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
    0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
    0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
    0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
    0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
    0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
    0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
    0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
    0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
    0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
    0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
    0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
    0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
    0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
    0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
    0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
    0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
    0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
    0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};


static uint16_t gen_crc16(const uint8_t *data, int length, unsigned short crc /*=0x0000*/) {
    for(int i = 0; i < length; i++) {
        uint16_t tableValue = crc16_table[(crc ^ *data++) & 0xff];
        crc = (crc >> 8) ^ tableValue;
    }
 
    return crc;
}


static void server_close(struct tcp_pcb *pcb)
{
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_close(pcb);

    printf("\nserver_close(): Closing...\n");
}
static err_t server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    //LWIP_UNUSED_ARG(arg);
    ModbusTcpRtu* m = (ModbusTcpRtu*)arg;

    if (err == ERR_OK && p != NULL)
    {
        tcp_recved(pcb, p->tot_len);

        printf("\nserver_recv(): pbuf->len is %d byte\n", p->len);
        printf("server_recv(): pbuf->tot_len is %d byte\n", p->tot_len);
        printf("server_recv(): pbuf->next is %d\n", p->next);

        if (p->len > 8)
        {
            if (m->outstanding_transaction)
            {
                if (((uint16_t*)p->payload)[0] == BIGENDIAN16(m->current_transaction))
                {
                    size_t len = BIGENDIAN16(((uint16_t*)p->payload)[2]);
                    uint16_t crc;
                    printf("MATCH %d\n", len);
                    char *buf = (char*)p->payload;
                    buf += 6; // Skip over header
                    crc = gen_crc16((const uint8_t*)buf, len, 0xffff);
                    m->serialdev->write(buf, len);
                    m->serialdev->write((const char*)&crc, sizeof(crc));
                    m->outstanding_transaction = false;
                }
            }
        }

        pbuf_free(p);
        //server_close(pcb);
    }
    else
    {
        printf("\nserver_recv(): Errors-> ");
        if (err != ERR_OK)
            printf("1) Connection is not on ERR_OK state, but in %d state->\n", err);
        if (p == NULL)
            printf("2) Pbuf pointer p is a NULL pointer->\n ");
        printf("server_recv(): Closing server-side connection...");

        pbuf_free(p);
        server_close(pcb);

        m->active_pcb = nullptr;
        m->clientConnectionFailed();
    }

    return ERR_OK;
}

static err_t server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    LWIP_UNUSED_ARG(len);
    LWIP_UNUSED_ARG(arg);

    printf("\nserver_sent(): Correctly ACKed\n");
    //server_close(pcb);

    return ERR_OK;
}

static err_t server_poll(void *arg, struct tcp_pcb *pcb)
{
    static int counter = 1;
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);

    printf("\nserver_poll(): Call number %d\n", counter++);
    return ERR_OK;
}

static void server_err(void *arg, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    printf("\nserver_err(): Fatal error, exiting...\n");
}


static err_t server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    ModbusTcpRtu* m = (ModbusTcpRtu*)arg;
    if (m->active_pcb != nullptr)
    {
        return ERR_USE;
    }
    m->active_pcb = newpcb;

    tcp_setprio(newpcb, TCP_PRIO_MIN);

    tcp_recv(newpcb, server_recv);
    //tcp_sent(pcb, server_sent);

    tcp_err(newpcb, server_err);

    tcp_poll(newpcb, server_poll, 4); //every two seconds of inactivity of the TCP connection

    tcp_accepted(newpcb);
    printf("\nserver_accept(): Accepting incoming connection on server %x...\n", arg);
    return ERR_OK;
}

err_t client_connected(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_setprio(newpcb, TCP_PRIO_MIN);

    tcp_recv(newpcb, server_recv);
    //tcp_sent(pcb, server_sent);

    tcp_err(newpcb, server_err);

    tcp_poll(newpcb, server_poll, 4); //every two seconds of inactivity of the TCP connection

    ModbusTcpRtu* m = (ModbusTcpRtu*)arg;
    m->active_pcb = newpcb;

    printf("\nclient_connected(): Connection established!\n");
    return ERR_OK;
}

void client_error(void * arg, err_t err)
{
    printf("\nclient_error()!\n");

    ModbusTcpRtu* m = (ModbusTcpRtu*)arg;
    m->clientConnectionFailed();
    m->active_pcb = nullptr;
}

void ModbusTcpRtu::configure(uint32_t ipaddress, uint16_t tcp_port, const uint8_t* rtu_slave_ids)
{
    ip4_addr_set_u32(&ip_address_, ipaddress);
    tcp_port_ = tcp_port;
    rtu_slave_ids_ = rtu_slave_ids;
}

void ModbusTcpRtu::clientConnectionFailed()
{
    if (ip_address_.addr != IPADDR_ANY)
    {
        connection_failed_at = millis();
        active_pcb = nullptr;
    }
}

// Establishes a new outbound connection
void ModbusTcpRtu::clientConnect()
{
    main_pcb = tcp_new();
    tcp_bind(main_pcb, IP_ADDR_ANY, /* source port */0);
    tcp_arg(main_pcb, this);
    tcp_err(main_pcb, client_error);
    tcp_connect(main_pcb, &ip_address_, tcp_port_, client_connected);
}

void ModbusTcpRtu::begin(uint32_t baud, uint16_t format)
{
    serialdev->transmitterEnable(tx_enable_pin_);
    serialdev->begin(baud, format);
    // crude calculation of how long 3.5 char times is in micros
    frame_spacing = (1000000 / (baud / 10)) * 35 / 10;
    printf("serial begin\n");
    rtu_rx_current = rtu_rx_buffer1;
    rtu_rx_pos = 0;

    if (ip_address_.addr == IPADDR_ANY && tcp_port_ != 0) {
        // TCP Server mode
        main_pcb = tcp_new();
        tcp_bind(main_pcb, IP_ADDR_ANY, tcp_port_);
        tcp_arg(main_pcb, this);
        main_pcb = tcp_listen(main_pcb);
        tcp_accept(main_pcb, server_accept);
    } else {
        // TCP Client mode
        clientConnect();
    }
}

void ModbusTcpRtu::end()
{
    serialdev->end();
}



typedef struct modbus_tcp_header
{
	uint16_t  transaction;
	uint16_t  protocol;
	uint16_t  remaining_len;
} modbus_tcp_header_t;


inline void ModbusTcpRtu::processRtuFrame()
{
    uint16_t expectedcrc;

    if (!rtu_rx_frame)
    {
        return;
    }

    char buf[3];
    unsigned char *p = (unsigned char *)rtu_rx_frame;
    for (size_t i = 0; i < rtu_rx_frame_len; i++)
    {
        sprintf((char *)&buf, "%02x", p[i]);
        printf((const char *)&buf);
    }
    printf("\n");
    if (rtu_rx_frame_len<4)
    {
        goto done;
    }
    //printf("CRC: %x\n", gen_crc16(rtu_rx_frame, rtu_rx_frame_len-2, 0xffff));
    expectedcrc = *(uint16_t*)(rtu_rx_frame+rtu_rx_frame_len - 2);
    if (expectedcrc ^ gen_crc16(rtu_rx_frame, rtu_rx_frame_len-2, 0xffff))
    {
        printf("CRC mismatch, dropping, should be %x\n", gen_crc16(rtu_rx_frame, rtu_rx_frame_len-2, 0xffff));
        goto done;
    }

    // Slave mode: discard all packages with the wrong address
    if (rtu_slave_ids_)
    {
        const uint8_t *uid = rtu_slave_ids_;
        bool found = *uid == 0 ? true : false; // Length of zero means no filter.
        while(*uid)
        {
            if (*uid++ == rtu_rx_frame[0])
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            printf("Ignoring, wrong address\n");
            goto done;
        }

        printf("NEW RTU FRAME\n");
        // Slave mode: assume Modbus RTU request, forward over TCP
        if (active_pcb)
        {
            // Expect it to be a response?
            current_transaction++;
            modbus_tcp_header_t hdr = {
                .transaction = BIGENDIAN16(current_transaction),
                .protocol = 0,
                .remaining_len = BIGENDIAN16((uint16_t)(rtu_rx_frame_len-2))
            };
            err_t lwip_err;
            if ((lwip_err = tcp_write(active_pcb, &hdr, sizeof(hdr), TCP_WRITE_FLAG_COPY)) != ERR_OK)
            {
                printf("E1 %x\n", lwip_err);
                goto done;
            }

            if ((lwip_err = tcp_write(active_pcb, rtu_rx_frame, rtu_rx_frame_len-2 /*skip crc*/, TCP_WRITE_FLAG_COPY)) != ERR_OK)
            {
                printf("E2 %x\n", lwip_err);
                goto done;
            }
            outstanding_transaction = true;
            if ((lwip_err = tcp_output(active_pcb)) != ERR_OK)
            {
                printf("E3 %x\n", lwip_err);
            }
        } else {
            printf("NO PCB\n");
        }
    }
done:
    rtu_rx_frame = nullptr;
    rtu_rx_frame_len = 0;
}

void ModbusTcpRtu::doYield()
{
    if (connection_failed_at != 0 && millis() - connection_failed_at > TCP_RECONNECTION_INTERVAL)
    {
        connection_failed_at = 0;
        clientConnect();
    }
    checkFrameReady(false);
}



void ModbusTcpRtu::checkFrameReady(bool serialEvent)
{
    uint32_t now = micros();
    if (now - last_serial_event >= frame_spacing && rtu_rx_pos > 0)
    {
        rtu_rx_frame = (uint8_t *)rtu_rx_current;
        rtu_rx_frame_len = rtu_rx_pos;
        // switch rx buffer
        rtu_rx_current = rtu_rx_current == rtu_rx_buffer1 ? rtu_rx_buffer2 : rtu_rx_buffer1;
        rtu_rx_pos = 0;

        
        if (!serialEvent)
        {
            processRtuFrame();
        }
    }
    if (serialEvent)
    {
        //printf("serialEvent %d\n", now - last_serial_event);
        last_serial_event = now;
    }
}
void ModbusTcpRtu::serialEvent()
{
    if (!rtu_rx_current)
    {
        return;
    }
    checkFrameReady(true);

    if (rtu_rx_pos < MODBUS_RTU_FRAME_SIZE)
    {
        rtu_rx_current[rtu_rx_pos++] = serialdev->read();
    }
}