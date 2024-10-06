
#pragma once

#include "ne2000.h"

#define FIFO_NE2K_SEND 0x88

extern uint8_t PM_EnableWifi();
extern void PM_Wifi_GetStatus();
extern void PM_RetryWifi();

extern uint8_t PM_NE2000_Read(uint8_t Addr);
extern void PM_NE2000_Write(uint8_t Addr,uint8_t Data);

//ne2000 functions used bu the PicoMEM Code
extern uint8_t ne2000_read(uint16_t address, void *p);
extern void ne2000_write(uint16_t address, uint8_t value, void *p);
extern uint8_t ne2000_asic_read(uint16_t offset, void *p);
extern void ne2000_asic_write(uint16_t offset, uint8_t value, void *p);

extern uint8_t ne2000_reset_read(uint16_t offset, void *p);

extern void ne2000_initiate_send();

extern ne2000_t *nic;

typedef struct wifi_infos_t {
       uint8_t cyw43_mac[6];
       char WIFI_SSID[33];
       char WIFI_KEY[63];
       char StatusStr[32];
       int16_t Status;
       int32_t rssi;           // Signal quality
       int16_t rate;           // rate ?
} wifi_infos_t;

/* Status values :
#define CYW43_LINK_DOWN         (0)     ///< link is down
#define CYW43_LINK_JOIN         (1)     ///< Connected to wifi
#define CYW43_LINK_NOIP         (2)     ///< Connected to wifi, but no IP address
#define CYW43_LINK_UP           (3)     ///< Connect to wifi with an IP address
#define CYW43_LINK_FAIL         (-1)    ///< Connection failed
#define CYW43_LINK_NONET        (-2)    ///< No matching SSID found (could be out of range, or down)
#define CYW43_LINK_BADAUTH      (-3)    ///< Authenticatation failure
*/