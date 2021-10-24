#include <Arduino.h>
#include "core_pins.h"
#include "debug/printf.h"
#include "usb_ecm.h"

#include "dhserver.h"
//#include "dnserver.h"
#include "netif/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/icmp.h"
#include "lwip/udp.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"

#include "serialmenu.h"

#include "configuration.h"
#include "modbus_tcp_rtu.h"

ModbusTcpRtu rtugw1 = ModbusTcpRtu(&Serial2, 9);
ModbusTcpRtu rtugw2 = ModbusTcpRtu(&Serial3, 19);
ModbusTcpRtu rtugw3 = ModbusTcpRtu(&Serial4, 18);

void serialEvent2()
{
  rtugw1.serialEvent();
}

void serialEvent3()
{
  rtugw2.serialEvent();
}

void serialEvent4()
{
  rtugw3.serialEvent();
}

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(*x))

static struct netif netif_data;
static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);
static struct pbuf *received_frame;

static dhcp_entry_t entries[] =
{
    /* mac ip address                          lease time */
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 2), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 3), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 4), 24 * 60 * 60 },
};

static const dhcp_config_t dhcp_config =
{
    .router = IPADDR4_INIT_BYTES(0, 0, 0, 0),  /* router address (if any) */
    .port = 67,                                /* listen port */
    .dns = IPADDR4_INIT_BYTES(192, 168, 7, 1), /* dns server (if any) */
    "sam",                                     /* dns suffix */
    ARRAY_SIZE(entries),                       /* num entry */
    entries                                    /* entries */
};

err_t output_fn(struct netif *netif, struct pbuf *p, const ip_addr_t *ipaddr)
{
  return etharp_output(netif, p, ipaddr);
}

static void usb_ecm_transmit_packet(struct pbuf *p)
{
  struct pbuf *q;
  size_t buf_size;

  while(!usb_ecm_can_transmit()) yield();
  char *buf = (char*)usb_ecm_write_start(&buf_size);
  char *buf_end = buf + buf_size;
  for(q = p; q != NULL; q = q->next)
  {

    //char printbuf[3];
    //for(size_t i=0; i<q->len; i++)
    //{
    //  sprintf((char*)&printbuf, "%02x", ((char *)q->payload)[i]);
    //  printf((const char*)&printbuf);
    //}

    u16_t seglen = q->len;
    while(seglen)
    {
      size_t to_copy = buf + seglen > buf_end ? buf_end - buf : seglen;
      memcpy(buf, (char*)q->payload + (q->len - seglen), to_copy);
      buf += to_copy;
      seglen -= to_copy;
      if (buf == buf_end)
      {
        usb_ecm_write_done(buf_size); // Entire buffer is filled
        while(!usb_ecm_can_transmit()) yield();
        buf = (char*)usb_ecm_write_start(&buf_size);
        buf_end = buf + buf_size;
      }
    }

    if (q->tot_len == q->len) break;
  }
  size_t last_size = buf - (buf_end - buf_size);
  usb_ecm_write_done(last_size);
}

err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
    for (;;)
    {
        if (usb_ecm_can_transmit()) goto ok_to_xmit;
        yield();
        sys_check_timeouts();
    }

    return ERR_USE;

ok_to_xmit:
    usb_ecm_transmit_packet(p);

    return ERR_OK;
}

err_t netif_init_cb(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = ECM_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->linkoutput = linkoutput_fn;
    netif->output = output_fn;
    return ERR_OK;
}

void init_lwip()
{
    struct netif  *netif = &netif_data;

    lwip_init();
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, (u_int8_t*)&HW_OCOTP_MAC0, 6);

    netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, ip_input);
    netif_set_default(netif);
}

void setup() {
  // put your setup code here, to run once:
  pinMode(13, OUTPUT);

  digitalWriteFast(13, HIGH);
  delay(500);
  digitalWriteFast(13, LOW);
  delay(500);


  while (!usb_configuration) yield();
  delay(5000);
  if (CrashReport)
  {
    CrashReport.printTo(Serial);
  }
  configuration_load();
  rtugw1.configure(active_config.modbus_gw[0].ipaddress, active_config.modbus_gw[0].tcp_port, active_config.modbus_gw[0].unit_filter);
  

  MenuSetup();
  init_lwip();

  while (!netif_is_up(&netif_data)) loop();

  while (dhserv_init(&dhcp_config) != ERR_OK) loop();

  rtugw1.begin(active_config.modbus_gw[0].baudrate);
  //while (dnserv_init(&ipaddr, 53, dns_query_proc) != ERR_OK);
}

void loop() {
  // put your main code here, to run repeatedly:
  size_t len;
  const void* packet = usb_ecm_read(&len);
  if (packet != NULL)
  {
    //printf("got packet with length %x\n", len);
    //char buf[3];
    //unsigned char *p = (unsigned char*)packet;
    //for(size_t i=0; i<len; i++)
    //{
    //  sprintf((char*)&buf, "%02x", p[i]);
    //  printf((const char*)&buf);
    //}
    //printf("\n");

    received_frame = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!received_frame) 
      return;

    memcpy(received_frame->payload, packet, len);
    ethernet_input(received_frame, &netif_data);
    usb_ecm_read_done();
  }
  sys_check_timeouts();

  rtugw1.doYield();
  rtugw2.doYield();
  rtugw3.doYield();
  yield();
  MenuPoll();
}


int main(void)
{
  setup();
  while(1)
  {
    loop();
  }
}

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void)
{
  return 0;
}
void sys_arch_unprotect(sys_prot_t pval)
{
  (void)pval;
}