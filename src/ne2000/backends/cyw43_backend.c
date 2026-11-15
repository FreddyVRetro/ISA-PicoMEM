/*
 * NE2000 backend: CYW43 WiFi (Pico W).
 *
 * Extracted from ne2000.c so the DP8390 emulator stays board-agnostic.
 * This TU owns:
 *   - cyw43_arch / cyw43_state interaction
 *   - PM_Wifi struct usage (status, SSID, MAC)
 *   - cyw43_cb_process_ethernet()  (driver RX callback -> ne2000_deliver_rx)
 *   - link up/down stubs the CYW43 driver requires
 *   - the ne2000_backend vtable instance
 *
 * Bring-up entry points (PM_ConnectWifi, PM_RetryWifi, PM_Wifi_GetStatus)
 * remain available with the same prototypes as before so the rest of
 * the firmware (pm_cmd.cpp, BIOS menu) keeps working unchanged.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "cyw43.h"
#include "cyw43_stats.h"

#include "ne2000.h"
#include "dev_ne2000.h"
#include "ne2000_backend.h"
#include "../pm_debug.h"
#include "pm_bluetooth_scan.h"
#include "f_util.h"
#include "ff.h"

// -- These were ad-hoc constants in ne2000.c; kept verbatim. --
#define WLC_GET_RATE ( (uint32_t)12 )
#define WLC_GET_RSSI ( (uint32_t)127 )

// Trim helper (was a static in ne2000.c).
static void cyw43be_trim(char *str) {
    int i = strlen(str) - 1;
    while (i >= 0 && (str[i] == '\r' || str[i] == '\n' || str[i] == ' ')) {
        str[i--] = '\0';
    }
}

static volatile bool link_up = false;

// ---------------------------------------------------------------------------
// Public WiFi management API (formerly in ne2000.c)
// ---------------------------------------------------------------------------

void PM_Wifi_GetStatus(void)
{
    PM_Wifi.Status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    cyw43_ioctl(&cyw43_state, (uint32_t)WLC_GET_RSSI<<1,
                sizeof(PM_Wifi.rssi), (uint8_t*)&PM_Wifi.rssi, CYW43_ITF_STA);
    cyw43_ioctl(&cyw43_state, (uint32_t)WLC_GET_RATE<<1,
                sizeof(PM_Wifi.rate), (uint8_t*)&PM_Wifi.rate, CYW43_ITF_STA);
    switch (PM_Wifi.Status) {
        case CYW43_LINK_DOWN:
            sprintf(PM_Wifi.StatusStr, " Err: Link Down"); break;
        case CYW43_LINK_JOIN:
        case CYW43_LINK_NOIP:
        case CYW43_LINK_UP:
            sprintf(PM_Wifi.StatusStr, " Connected, Signal %d dB", PM_Wifi.rssi); break;
        case CYW43_LINK_FAIL:
            sprintf(PM_Wifi.StatusStr, " Err: Connection failed"); break;
        case CYW43_LINK_NONET:
            sprintf(PM_Wifi.StatusStr, " Err: SSID not found"); break;
        case CYW43_LINK_BADAUTH:
            sprintf(PM_Wifi.StatusStr, " Err: Authentication failure"); break;
        default:
            break;
    }
}

void PM_RetryWifi(void)
{
    cyw43_arch_wifi_connect_async(PM_Wifi.WIFI_SSID, PM_Wifi.WIFI_KEY,
                                  PM_Wifi.WIFI_KEY[0] ? CYW43_AUTH_WPA2_MIXED_PSK
                                                      : CYW43_AUTH_OPEN);
}

uint8_t PM_ConnectWifi(void)
{
    FIL WifiConfigFile;
    FRESULT fr;

    PM_INFO("cyw43_arch_init();\n");
    int cyw43_err = cyw43_arch_init();
    if (cyw43_err != 0) {
        PM_ERROR("Init failed: %d\n", cyw43_err);
        sprintf(PM_Wifi.StatusStr, "Init failed: %d\n", cyw43_err);
        return 1;
    }
#if PM_WIFI_LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
#endif

    PM_INFO("cyw43_arch_enable_sta_mode();\n");
    cyw43_arch_enable_sta_mode();
    if (!pm_bluetooth_scan_init()) {
        PM_ERROR("[BT] Scanner initialization failed\n");
    }
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, CYW43_COUNTRY_CANADA);
    cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, (uint8_t *)&PM_Wifi.cyw43_mac);

    if ((PM_Wifi.cyw43_mac[0] == 0) && (PM_Wifi.cyw43_mac[1] == 0) &&
        (PM_Wifi.cyw43_mac[2] == 0) && (PM_Wifi.cyw43_mac[3] == 0)) {
        PM_ERROR("No MAC > Not initialized\n");
        sprintf(PM_Wifi.StatusStr, "No MAC > Not initialized\n");
        return 1;
    }

    PM_INFO("[WIFI] MAC Address: %02X.%02X.%02X.%02X.%02X.%02X\n\r",
            PM_Wifi.cyw43_mac[0], PM_Wifi.cyw43_mac[1], PM_Wifi.cyw43_mac[2],
            PM_Wifi.cyw43_mac[3], PM_Wifi.cyw43_mac[4], PM_Wifi.cyw43_mac[5]);

    fr = f_open(&WifiConfigFile, "Wifi.txt", FA_OPEN_EXISTING | FA_READ);
    if (FR_OK != fr) {
        PM_ERROR("Can't open wifi.txt %d\n", fr);
        sprintf(PM_Wifi.StatusStr, "Can't open wifi.txt");
        return 2;
    }
    PM_Wifi.WIFI_KEY[0] = '\0';
    bool got_ssid = f_gets(PM_Wifi.WIFI_SSID, sizeof PM_Wifi.WIFI_SSID,
                           &WifiConfigFile) != NULL;
    if (got_ssid) {
        f_gets(PM_Wifi.WIFI_KEY, sizeof PM_Wifi.WIFI_KEY, &WifiConfigFile);
    }
    f_close(&WifiConfigFile);
    if (got_ssid) {
        cyw43be_trim(PM_Wifi.WIFI_SSID);
        cyw43be_trim(PM_Wifi.WIFI_KEY);
        if (strlen(PM_Wifi.WIFI_SSID)) {
            uint32_t auth = PM_Wifi.WIFI_KEY[0] ? CYW43_AUTH_WPA2_MIXED_PSK
                                                : CYW43_AUTH_OPEN;
            PM_INFO("[WIFI] Joining SSID: %s (%s)\n", PM_Wifi.WIFI_SSID,
                    PM_Wifi.WIFI_KEY[0] ? "secured" : "open");
            (void)cyw43_arch_wifi_connect_async(PM_Wifi.WIFI_SSID, PM_Wifi.WIFI_KEY,
                                                auth);
            sleep_us(1000);  // tiny gap before changing power-save mode
            cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);
        } else {
            PM_ERROR("[WIFI] Missing SSID");
            sprintf(PM_Wifi.StatusStr, "Missing SSID");
            return 3;
        }
    } else {
#if PM_PRINTF
        PM_ERROR("WIFI Configuration file Error.");
#endif
        sprintf(PM_Wifi.StatusStr, "Configuration file Error");
        return 4;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CYW43 driver hooks (the lwIP-less shim left by the original code)
// ---------------------------------------------------------------------------

#define NOLWIP
#ifdef NOLWIP
// Linker stub when lwIP is not pulled in.
uint16_t pbuf_copy_partial(void *buf, void *dataptr, uint16_t len, uint16_t offset)
{
    (void)buf; (void)dataptr; (void)len; (void)offset;
    return 0;
}

void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf) {
    (void)self; (void)itf;
#if PM_PRINTF
    PM_INFO("Link Up\n");
#endif
    link_up = true;
#if PM_WIFI_LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, link_up);
#endif
}

void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf) {
    (void)self; (void)itf;
#if PM_PRINTF
    PM_INFO("Link Down\n");
#endif
    link_up = false;
#if PM_WIFI_LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, link_up);
#endif
}
#endif

// CYW43 driver -> NE2000 emulator. Push-model RX delivery.
void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf)
{
    (void)cb_data; (void)itf;
    ne2000_deliver_rx(buf, (uint16_t)len);
    CYW43_STAT_INC(PACKET_IN_COUNT);
}

// ---------------------------------------------------------------------------
// Backend vtable
// ---------------------------------------------------------------------------

static void cyw43be_send(const uint8_t *frame, uint16_t len)
{
    cyw43_send_ethernet(&cyw43_state, CYW43_ITF_STA,
                        len, frame, false);
}

ne2000_backend_t ne2000_backend = {
    .name = "CYW43",
    .send = cyw43be_send,
    .poll = NULL,   // CYW43 is push-model via cyw43_cb_process_ethernet
};
