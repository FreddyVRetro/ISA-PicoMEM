
#pragma once

#include "ne2000.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO_NE2K_SEND 0x88

extern uint8_t PM_ConnectWifi();
extern void PM_Wifi_GetStatus();
extern void PM_RetryWifi();

void dev_ne2000_cfg(uint16_t port,uint8_t irq);
extern void dev_ne2000_quiesce(void);   // POST/reboot: stop card, mask, release IRQ
extern bool dev_ne2000_ior(uint32_t Addr,uint8_t *Data);
extern void dev_ne2000_iow(uint32_t Addr,uint8_t Data);

//ne2000 functions used by the PicoMEM Code
extern uint8_t ne2000_read(uint16_t address, void *p);
extern void ne2000_write(uint16_t address, uint8_t value, void *p);
extern uint8_t ne2000_asic_read(uint16_t offset, void *p);
extern void ne2000_asic_write(uint16_t offset, uint8_t value, void *p);
extern uint8_t ne2000_reset_read(uint16_t offset, void *p);

extern void ne2000_initiate_send();

#ifdef __cplusplus
}
#endif

typedef struct wifi_infos_t {
       uint8_t cyw43_mac[6];
       char WIFI_SSID[33];
       char WIFI_KEY[63];
       char StatusStr[32];
       int16_t Status;
       int32_t rssi;           // Signal quality
       int16_t rate;           // rate ?
       bool initialized;       // true if cyw43 is initialized
       // DHCP-snooped lease (DOS/mTCP owns the IP; the Pico has none in
       // arch_none/no-lwIP, so we learn it by watching the DHCP ACK go by).
       // Big-endian-in-uint32 (octet1 in the high byte) to match the
       // screenpad's fmt_ip(). 0 = not seen yet.
       uint32_t dhcp_ip;       // yiaddr (leased address)
       uint32_t dhcp_mask;     // option 1 (subnet mask)
       uint32_t dhcp_gw;       // option 3 (router)
       uint32_t dhcp_dns;      // option 6 (first DNS)
} wifi_infos_t;

extern ne2000_t *nic;
extern wifi_infos_t PM_Wifi;   // Wifi Info Structure for PicoMEM

/* Status values :
#define CYW43_LINK_DOWN         (0)     ///< link is down
#define CYW43_LINK_JOIN         (1)     ///< Connected to wifi
#define CYW43_LINK_NOIP         (2)     ///< Connected to wifi, but no IP address
#define CYW43_LINK_UP           (3)     ///< Connect to wifi with an IP address
#define CYW43_LINK_FAIL         (-1)    ///< Connection failed
#define CYW43_LINK_NONET        (-2)    ///< No matching SSID found (could be out of range, or down)
#define CYW43_LINK_BADAUTH      (-3)    ///< Authenticatation failure
*/