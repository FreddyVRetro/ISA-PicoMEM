#include <string.h>

#include "btstack.h"
#include "pico/btstack_cyw43.h"
#include "pico/cyw43_arch.h"

#include "pm_bluetooth.h"
#include "pm_bluetooth_scan.h"
#include "../pm_debug.h"
#include "inputs/buttons.h"
#include "inputs/input_event.h"
#include "inputs/router/router.h"
#include "inputs/pm_keyboard.h"

/*
 * Generic HID report descriptor parser, shared with the USB gamepad driver
 * (USB_Host_Driver2/hid/devices/generic). It is transport-agnostic: it only
 * needs a descriptor buffer and hands back a flat list of report items, so
 * it is reused here to decode BLE HOGP Report Map descriptors for generic
 * (non-boot) joypads/gamepads without touching any USB-owned device state.
 *
 * Note: tusb.h is deliberately NOT included here -- it defines its own
 * hid_report_type_t which collides with BTstack's (from btstack_hid.h,
 * pulled in via btstack.h above). Only the small set of standard USB HID
 * Usage Page/Usage constants actually needed are replicated below (stable
 * values from the USB HID Usage Tables spec).
 */
#include "USB_Host_Driver2/hid/devices/generic/hid_parser.h"

#define HID_USAGE_PAGE_DESKTOP     0x01
#define HID_USAGE_PAGE_BUTTON      0x09
#define HID_USAGE_DESKTOP_X        0x30
#define HID_USAGE_DESKTOP_Y        0x31
#define HID_USAGE_DESKTOP_Z        0x32
#define HID_USAGE_DESKTOP_RZ       0x35
#define HID_USAGE_DESKTOP_HAT_SWITCH 0x39

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static bool scanner_initialized;
static bool pairing_requested;
static uint8_t hid_descriptor_storage[512];
static char scan_status[64] = "Scanner starting";
static uint8_t selected_device_index = PM_BLUETOOTH_MAX_DEVICES;
static bool btstack_working;
static btstack_timer_source_t scan_timer;
static uint8_t unknown_device_addresses[PM_BLUETOOTH_MAX_DEVICES][6];
static uint8_t unknown_device_count;

#define PM_BLUETOOTH_SCAN_DURATION_MS 5000
#define PM_BT_JOYPAD_MAX_BUTTONS 12

// Debug helper: prints up to 32 bytes of a buffer as hex, one line, prefixed
// with the given tag. Used to log raw HOGP descriptors/reports for joypad
// bring-up so mismatches between what BTstack parses and what a real
// device sends can be diagnosed from the serial log alone.
static void pm_bluetooth_hex_dump(const char *tag, const uint8_t *data, uint16_t len)
{
    char line[3 * 32 + 1];
    uint16_t dump_len = len > 32 ? 32 : len;

    for (uint16_t i = 0; i < dump_len; ++i) {
        snprintf(&line[i * 3], 4, "%02X ", data[i]);
    }
    line[dump_len * 3] = '\0';
    PM_INFO("[BT] %s (%u bytes): %s%s\n", tag, len, line, len > 32 ? "..." : "");
}

typedef enum {
    PM_BT_WAITING,
    PM_BT_SCANNING,
    PM_BT_CONNECTING,
    PM_BT_PAIRING,
} pm_bluetooth_state_t;

// Descriptor-parsed location of a single report field (axis or button).
typedef struct {
    uint8_t byteIndex;
    uint16_t bitMask;
    uint32_t max;
    int32_t min;
} pm_bt_field_loc_t;

// Per-role BLE HID connection. Mouse and keyboard use HID_PROTOCOL_MODE_BOOT
// (standard fixed-format reports); joypad uses HID_PROTOCOL_MODE_REPORT and
// its Report Map descriptor is parsed to find button/axis locations.
typedef struct {
    bool active;                              // HID service connected and usable
    hci_con_handle_t connection_handle;
    uint16_t hids_cid;
    pm_bluetooth_device_type_t type;
    char status[32];

    // Joypad-only descriptor-derived report layout (report id 0 assumed).
    pm_bt_field_loc_t xLoc, yLoc, zLoc, rzLoc, hatLoc;
    pm_bt_field_loc_t buttonLoc[PM_BT_JOYPAD_MAX_BUTTONS];
    uint8_t buttonCnt;
} pm_bluetooth_connection_t;

static pm_bluetooth_connection_t bt_conn[PM_BT_SLOT_COUNT];
static pm_bluetooth_slot_t pending_slot = PM_BT_SLOT_COUNT; // COUNT == "none pending"

static pm_bluetooth_state_t bluetooth_state;

static const char *pm_bluetooth_slot_names[PM_BT_SLOT_COUNT] = {
    "Mouse", "Keyboard", "Joypad",
};

const char *pm_bluetooth_slot_name(pm_bluetooth_slot_t slot)
{
    return slot < PM_BT_SLOT_COUNT ? pm_bluetooth_slot_names[slot] : "?";
}

const char *pm_bluetooth_slot_status(pm_bluetooth_slot_t slot)
{
    return slot < PM_BT_SLOT_COUNT ? bt_conn[slot].status : "";
}

// Maps a classified device type to the connection slot it should occupy.
// Generic HID-service-only devices (no Appearance) default to the joypad
// slot since gamepads are the class most likely to omit Appearance.
static bool pm_bluetooth_slot_for_type(pm_bluetooth_device_type_t type, pm_bluetooth_slot_t *slot)
{
    switch (type) {
    case PM_BLUETOOTH_DEVICE_MOUSE:
        *slot = PM_BT_SLOT_MOUSE;
        return true;
    case PM_BLUETOOTH_DEVICE_KEYBOARD:
        *slot = PM_BT_SLOT_KEYBOARD;
        return true;
    case PM_BLUETOOTH_DEVICE_JOYPAD:
    case PM_BLUETOOTH_DEVICE_HID:
        *slot = PM_BT_SLOT_JOYPAD;
        return true;
    default:
        return false;
    }
}

static void pm_bluetooth_connection_reset(pm_bluetooth_slot_t slot)
{
    bt_conn[slot].active = false;
    bt_conn[slot].connection_handle = HCI_CON_HANDLE_INVALID;
    bt_conn[slot].hids_cid = 0;
    bt_conn[slot].type = PM_BLUETOOTH_DEVICE_UNKNOWN;
    strcpy(bt_conn[slot].status, "Not connected");
    memset(&bt_conn[slot].xLoc, 0, sizeof(bt_conn[slot].xLoc));
    memset(&bt_conn[slot].yLoc, 0, sizeof(bt_conn[slot].yLoc));
    memset(&bt_conn[slot].zLoc, 0, sizeof(bt_conn[slot].zLoc));
    memset(&bt_conn[slot].rzLoc, 0, sizeof(bt_conn[slot].rzLoc));
    memset(&bt_conn[slot].hatLoc, 0, sizeof(bt_conn[slot].hatLoc));
    memset(bt_conn[slot].buttonLoc, 0, sizeof(bt_conn[slot].buttonLoc));
    bt_conn[slot].buttonCnt = 0;
}

// Finds the slot whose HIDS client id or connection handle matches, or
// PM_BT_SLOT_COUNT if none does.
static pm_bluetooth_slot_t pm_bluetooth_slot_for_hids_cid(uint16_t cid)
{
    for (pm_bluetooth_slot_t slot = 0; slot < PM_BT_SLOT_COUNT; ++slot) {
        if (bt_conn[slot].hids_cid == cid && bt_conn[slot].connection_handle != HCI_CON_HANDLE_INVALID) {
            return slot;
        }
    }
    return PM_BT_SLOT_COUNT;
}

static pm_bluetooth_slot_t pm_bluetooth_slot_for_connection(hci_con_handle_t handle)
{
    for (pm_bluetooth_slot_t slot = 0; slot < PM_BT_SLOT_COUNT; ++slot) {
        if (bt_conn[slot].connection_handle == handle) {
            return slot;
        }
    }
    return PM_BT_SLOT_COUNT;
}

static void pm_bluetooth_scan_timeout(btstack_timer_source_t *timer)
{
    (void)timer;

    if (bluetooth_state == PM_BT_SCANNING) {
        gap_stop_scan();
        bluetooth_state = PM_BT_WAITING;
        pairing_requested = false;
        pending_slot = PM_BT_SLOT_COUNT;
        selected_device_index = PM_BLUETOOTH_MAX_DEVICES;
        strcpy(scan_status, "Scan complete");
        PM_INFO("[BT] Scan stopped after %d ms\n", PM_BLUETOOTH_SCAN_DURATION_MS);
    }
}

static void pm_bluetooth_start_scan(void)
{
    /*
     * Keep scanning active while pairing too. Many devices put their name
     * and identifying data in SCAN_RSP rather than the initial ADV packet.
     * Passive scanning would never receive that response.
     */
    gap_set_scan_parameters(1, 48, 48);
    gap_start_scan();
    bluetooth_state = PM_BT_SCANNING;
    if (pairing_requested && pending_slot < PM_BT_SLOT_COUNT) {
        snprintf(scan_status, sizeof(scan_status), "Scanning for BLE %s",
                 pm_bluetooth_slot_name(pending_slot));
    } else {
        strcpy(scan_status, "Scanning");
    }
    btstack_run_loop_set_timer_handler(&scan_timer, pm_bluetooth_scan_timeout);
    btstack_run_loop_set_timer(&scan_timer, PM_BLUETOOTH_SCAN_DURATION_MS);
    btstack_run_loop_add_timer(&scan_timer);
    PM_INFO("[BT] %s\n", scan_status);
}

static void pm_bluetooth_get_name(const uint8_t *advertisement, uint8_t length,
                                  char name[PM_BLUETOOTH_NAME_LEN])
{
    ad_context_t context;

    strcpy(name, "Unknown");
    for (ad_iterator_init(&context, length, (uint8_t *)advertisement);
         ad_iterator_has_more(&context);
         ad_iterator_next(&context)) {
        uint8_t type = ad_iterator_get_data_type(&context);
        uint8_t name_length = ad_iterator_get_data_len(&context);

        if (type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME ||
            type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME) {
            if (name_length >= PM_BLUETOOTH_NAME_LEN) {
                name_length = PM_BLUETOOTH_NAME_LEN - 1;
            }
            memcpy(name, ad_iterator_get_data(&context), name_length);
            name[name_length] = '\0';
            return;
        }
    }
}

static bool pm_bluetooth_name_contains(const char *name, const char *text)
{
    size_t text_length = strlen(text);

    for (; *name; ++name) {
        size_t index;
        for (index = 0; index < text_length && name[index]; ++index) {
            char actual = name[index];
            char expected = text[index];
            if (actual >= 'A' && actual <= 'Z') actual = (char)(actual - 'A' + 'a');
            if (expected >= 'A' && expected <= 'Z') expected = (char)(expected - 'A' + 'a');
            if (actual != expected) break;
        }
        if (index == text_length) return true;
    }
    return false;
}

static pm_bluetooth_device_type_t pm_bluetooth_get_type(const uint8_t *advertisement,
                                                         uint8_t length,
                                                         const char *name)
{
    ad_context_t context;
    bool hid = ad_data_contains_uuid16(length, advertisement,
                                       ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);

    for (ad_iterator_init(&context, length, (uint8_t *)advertisement);
         ad_iterator_has_more(&context);
         ad_iterator_next(&context)) {
        if (ad_iterator_get_data_type(&context) == BLUETOOTH_DATA_TYPE_APPEARANCE &&
            ad_iterator_get_data_len(&context) >= 2) {
            uint16_t appearance = little_endian_read_16(ad_iterator_get_data(&context), 0);
            /*
             * The upper ten bits identify the Appearance category. The lower
             * six bits are a subtype and new subtypes may be assigned without
             * changing the category, so do not compare the full value except
             * where the category itself is not enough to distinguish devices.
             */
            switch (appearance & 0xffc0) {
            case 0x03c0: /* HID */
                switch (appearance & 0x003f) {
                case 1: return PM_BLUETOOTH_DEVICE_KEYBOARD;
                case 2: return PM_BLUETOOTH_DEVICE_MOUSE;
                case 3: /* Joystick */
                case 4: /* Gamepad */
                    return PM_BLUETOOTH_DEVICE_JOYPAD;
                default:
                    break;
                }
                break;
            case 0x0840: /* Audio sink */
                return PM_BLUETOOTH_DEVICE_SPEAKER;
            case 0x0940: /* Wearable audio device */
                /* Earbud, headset, headphones, and neck band. */
                if ((appearance & 0x003f) >= 1 && (appearance & 0x003f) <= 4) {
                    return PM_BLUETOOTH_DEVICE_HEADPHONES;
                }
                break;
            case 0x0040: return PM_BLUETOOTH_DEVICE_PHONE;
            case 0x0080: return PM_BLUETOOTH_DEVICE_COMPUTER;
            case 0x00c0: return PM_BLUETOOTH_DEVICE_WATCH;
            case 0x0300: return PM_BLUETOOTH_DEVICE_THERMOMETER;
            case 0x0340: return PM_BLUETOOTH_DEVICE_HEART_RATE;
            default:
                break;
            }
        }
    }

    if (hid) return PM_BLUETOOTH_DEVICE_HID;

    /*
     * Some HID devices (including several low-cost mice) omit both the
     * Appearance and HID service UUID from their advertisements. Only use
     * unambiguous name tokens as a fallback; otherwise retain Unknown.
     */
    if (pm_bluetooth_name_contains(name, "mouse") ||
        pm_bluetooth_name_contains(name, "trackball")) {
        return PM_BLUETOOTH_DEVICE_MOUSE;
    }
    if (pm_bluetooth_name_contains(name, "keyboard") ||
        pm_bluetooth_name_contains(name, "keychron") ||
        pm_bluetooth_name_contains(name, "kbd")) {
        return PM_BLUETOOTH_DEVICE_KEYBOARD;
    }
    if (pm_bluetooth_name_contains(name, "gamepad") ||
        pm_bluetooth_name_contains(name, "joystick") ||
        pm_bluetooth_name_contains(name, "controller")) {
        return PM_BLUETOOTH_DEVICE_JOYPAD;
    }
    if (pm_bluetooth_name_contains(name, "headphone") ||
        pm_bluetooth_name_contains(name, "headset") ||
        pm_bluetooth_name_contains(name, "earbud") ||
        pm_bluetooth_name_contains(name, "airpod")) {
        return PM_BLUETOOTH_DEVICE_HEADPHONES;
    }

    return PM_BLUETOOTH_DEVICE_UNKNOWN;
}

static uint16_t pm_bluetooth_get_appearance(const uint8_t *advertisement, uint8_t length)
{
    ad_context_t context;

    for (ad_iterator_init(&context, length, (uint8_t *)advertisement);
         ad_iterator_has_more(&context);
         ad_iterator_next(&context)) {
        if (ad_iterator_get_data_type(&context) == BLUETOOTH_DATA_TYPE_APPEARANCE &&
            ad_iterator_get_data_len(&context) >= 2) {
            return little_endian_read_16(ad_iterator_get_data(&context), 0);
        }
    }
    return 0xffff;
}

static bool pm_bluetooth_has_hid_service(const uint8_t *advertisement, uint8_t length)
{
    return ad_data_contains_uuid16(length, advertisement,
                                   ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
}

static const char *pm_bluetooth_type_source(uint16_t appearance, bool hid_service,
                                            pm_bluetooth_device_type_t type)
{
    if (appearance != 0xffff) return "appearance";
    if (hid_service) return "hid-service";
    if (type != PM_BLUETOOTH_DEVICE_UNKNOWN) return "name";
    return "none";
}

static void pm_bluetooth_log_unknown_device(const uint8_t address[6],
                                            const uint8_t *advertisement, uint8_t length)
{
    ad_context_t context;

    for (uint8_t index = 0; index < unknown_device_count; ++index) {
        if (memcmp(unknown_device_addresses[index], address, sizeof(unknown_device_addresses[index])) == 0) {
            return;
        }
    }
    if (unknown_device_count == PM_BLUETOOTH_MAX_DEVICES) {
        return;
    }

    memcpy(unknown_device_addresses[unknown_device_count++], address, sizeof(unknown_device_addresses[0]));
    PM_INFO("[BT] Unknown %02X:%02X:%02X:%02X:%02X:%02X AD:",
            address[0], address[1], address[2], address[3], address[4], address[5]);
    for (ad_iterator_init(&context, length, (uint8_t *)advertisement);
         ad_iterator_has_more(&context);
         ad_iterator_next(&context)) {
        uint8_t type = ad_iterator_get_data_type(&context);
        uint8_t data_len = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);

        PM_INFO(" %02X(%d)", type, data_len);
        if (type == BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS ||
            type == BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS) {
            for (uint8_t offset = 0; offset + 1 < data_len; offset += 2) {
                PM_INFO("=%04X", little_endian_read_16(data, offset));
            }
        } else if (type == BLUETOOTH_DATA_TYPE_SERVICE_DATA && data_len >= 2) {
            PM_INFO("=%04X", little_endian_read_16(data, 0));
        }
    }
    PM_INFO("\n");
    PM_INFO("[BT] Unknown raw AD:");
    for (uint8_t offset = 0; offset < length; ++offset) {
        PM_INFO(" %02X", advertisement[offset]);
    }
    PM_INFO("\n");
}

static void pm_bluetooth_packet_handler(uint8_t packet_type, uint16_t channel,
                                        uint8_t *packet, uint16_t size)
{
    bd_addr_t address;
    char name[PM_BLUETOOTH_NAME_LEN];
    uint8_t length;
    const uint8_t *advertisement;
    pm_bluetooth_device_type_t type;
    uint16_t appearance;
    bool hid_service;
    uint8_t advertising_event_type;
    int8_t rssi;

    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
            btstack_working = true;
            strcpy(scan_status, "Ready to scan");
        }
        return;
    case HCI_EVENT_META_GAP:
        if (hci_event_gap_meta_get_subevent_code(packet) == GAP_SUBEVENT_LE_CONNECTION_COMPLETE &&
            bluetooth_state == PM_BT_CONNECTING && pending_slot < PM_BT_SLOT_COUNT) {
            bt_conn[pending_slot].connection_handle =
                gap_subevent_le_connection_complete_get_connection_handle(packet);
            bluetooth_state = PM_BT_PAIRING;
            snprintf(scan_status, sizeof(scan_status), "Pairing %s",
                     pm_bluetooth_slot_name(pending_slot));
            strcpy(bt_conn[pending_slot].status, "Pairing");
            PM_INFO("[BT] BLE %s connected; requesting pairing\n",
                    pm_bluetooth_slot_name(pending_slot));
            sm_request_pairing(bt_conn[pending_slot].connection_handle);
        }
        return;
    case HCI_EVENT_DISCONNECTION_COMPLETE: {
        hci_con_handle_t handle = hci_event_disconnection_complete_get_connection_handle(packet);
        pm_bluetooth_slot_t slot = pm_bluetooth_slot_for_connection(handle);

        if (slot < PM_BT_SLOT_COUNT) {
            PM_INFO("[BT] BLE %s disconnected\n", pm_bluetooth_slot_name(slot));
            pm_bluetooth_connection_reset(slot);
            if (slot == pending_slot) {
                pending_slot = PM_BT_SLOT_COUNT;
                pairing_requested = false;
                selected_device_index = PM_BLUETOOTH_MAX_DEVICES;
                bluetooth_state = PM_BT_WAITING;
            }
            // Don't disturb an unrelated in-flight pairing operation.
            if (bluetooth_state == PM_BT_WAITING) {
                pm_bluetooth_start_scan();
            }
        }
        return;
    }
    case GAP_EVENT_ADVERTISING_REPORT: {
        // pm_bluetooth_get_name() (and the rest of the advertising-report
        // handling below) only ever runs here: every other case above
        // returns, so this case's break is the sole path that reaches the
        // shared code following the switch. Scoped explicitly in this
        // block so that stays true even if a future case is added above.
        gap_event_advertising_report_get_address(packet, address);
        length = gap_event_advertising_report_get_data_length(packet);
        advertisement = gap_event_advertising_report_get_data(packet);
        pm_bluetooth_get_name(advertisement, length, name);
        type = pm_bluetooth_get_type(advertisement, length, name);
        appearance = pm_bluetooth_get_appearance(advertisement, length);
        hid_service = pm_bluetooth_has_hid_service(advertisement, length);
        advertising_event_type =
            gap_event_advertising_report_get_advertising_event_type(packet);
        rssi = (int8_t)gap_event_advertising_report_get_rssi(packet);
        if (type == PM_BLUETOOTH_DEVICE_UNKNOWN) {
            pm_bluetooth_log_unknown_device(address, advertisement, length);
        }
        pm_bluetooth_record_device_info(address, name, rssi,
                                        gap_event_advertising_report_get_address_type(packet), type,
                                        appearance, hid_service, length);
        PM_INFO("[BT] Detect %02X:%02X:%02X:%02X:%02X:%02X name=%s type=%d "
                "source=%s appearance=%s hid=%d ad_len=%d addr_type=%d event=%d%s\n",
                address[0], address[1], address[2], address[3], address[4], address[5],
                name, type, pm_bluetooth_type_source(appearance, hid_service, type),
                appearance == 0xffff ? "none" : "present",
                hid_service, length,
                gap_event_advertising_report_get_address_type(packet),
                advertising_event_type,
                advertising_event_type == 0x04 ? " (scan response)" : "");
        if (appearance != 0xffff) {
            PM_INFO("[BT] Appearance=0x%04X\n", appearance);
        }
        PM_INFO("[BT] %02X:%02X:%02X:%02X:%02X:%02X %s %d dBm\n",
                address[0], address[1], address[2], address[3], address[4], address[5],
                name, rssi);

        if (selected_device_index < pm_bluetooth_device_count() &&
            bluetooth_state == PM_BT_SCANNING) {
            const pm_bluetooth_device_t *device = pm_bluetooth_get_device(selected_device_index);

            if (memcmp(device->address, address, sizeof(address)) != 0) {
                return;
            }
            btstack_run_loop_remove_timer(&scan_timer);
            gap_stop_scan();
            bluetooth_state = PM_BT_CONNECTING;
            snprintf(scan_status, sizeof(scan_status), "Connecting to BLE %s",
                     pm_bluetooth_slot_name(pending_slot));
            PM_INFO("[BT] Connecting to selected BLE %s device\n", pm_bluetooth_slot_name(pending_slot));
            gap_connect(address, device->address_type);
        }
        return;
    }
    default:
        return;
    }
}

static void pm_bluetooth_mouse_report(const pm_bluetooth_connection_t *conn,
                                      const uint8_t *report, uint16_t report_len)
{
    uint32_t buttons = 0;

    if (report_len < 3) {
        return;
    }

    if (report[0] & 0x01) buttons |= JP_BUTTON_B1;
    if (report[0] & 0x02) buttons |= JP_BUTTON_B2;
    if (report[0] & 0x04) buttons |= JP_BUTTON_S2;

    input_event_t event = {
        .dev_addr = (uint8_t)conn->connection_handle,
        .instance = 0,
        .type = INPUT_TYPE_MOUSE,
        .transport = INPUT_TRANSPORT_BT_BLE,
        .buttons = buttons,
        .delta_x = (int8_t)report[1],
        .delta_y = (int8_t)report[2],
        .delta_wheel = report_len > 3 ? (int8_t)report[3] : 0,
    };

    router_submit_input(&event);
}

// Standard HID boot keyboard report: byte 0 = modifier, byte 1 = reserved,
// bytes 2..7 = up to 6 currently-pressed keycodes. Reused verbatim from the
// USB keyboard path (pm_keyboard_submit_report drives the PS/2 scan code
// stream directly; the router's keyboard event type can't carry all 6 keys
// plus the modifier byte, see pm_keyboard.h).
static void pm_bluetooth_keyboard_report(const uint8_t *report, uint16_t report_len)
{
    if (report_len < 8) {
        return;
    }
    pm_keyboard_submit_report(report[0], &report[2]);
}

// Parses a BLE HOGP Report Map descriptor (fetched after HID service connect)
// into button/axis byte+bitmask locations, using the same generic HID
// report-item parser the USB gamepad driver uses. Assumes a single Report ID
// (or none), which covers the vast majority of simple BLE gamepads/joysticks.
static void pm_bluetooth_joypad_parse_descriptor(pm_bluetooth_connection_t *conn,
                                                 const uint8_t *descriptor, uint16_t descriptor_len)
{
    HID_ReportInfo_t *info = NULL;

    pm_bluetooth_hex_dump("Joypad Report Map descriptor", descriptor, descriptor_len);

    memset(&conn->xLoc, 0, sizeof(conn->xLoc));
    memset(&conn->yLoc, 0, sizeof(conn->yLoc));
    memset(&conn->zLoc, 0, sizeof(conn->zLoc));
    memset(&conn->rzLoc, 0, sizeof(conn->rzLoc));
    memset(&conn->hatLoc, 0, sizeof(conn->hatLoc));
    memset(conn->buttonLoc, 0, sizeof(conn->buttonLoc));
    conn->buttonCnt = 0;

    if (USB_ProcessHIDReport(0, 0, descriptor, descriptor_len, &info) != HID_PARSE_Successful || info == NULL) {
        PM_INFO("[BT] Joypad descriptor parse failed\n");
        if (info) USB_FreeReportInfo(info);
        return;
    }

    /*
     * Unlike USB (where a Report ID byte is sometimes prefixed onto the
     * report buffer by the host stack), BLE HOGP report characteristics
     * carry only the report body for their own Report ID -- no ID prefix --
     * so BitOffset is used as-is with no extra offset.
     */
    for (HID_ReportItem_t *item = info->FirstReportItem; item != NULL; item = item->Next) {
        uint8_t bitSize = item->Attributes.BitSize;
        uint16_t bitOffset = item->BitOffset;
        uint16_t bitMask = (uint16_t)((0xFFFFu >> (16 - bitSize)) << (bitOffset % 8));
        uint8_t byteIndex = (uint8_t)(bitOffset / 8);

        PM_INFO("[BT]   item page=0x%02X usage=0x%02X bitOffset=%u bitSize=%u byteIndex=%u bitMask=0x%04X min=%ld max=%ld\n",
                item->Attributes.Usage.Page, item->Attributes.Usage.Usage, bitOffset, bitSize,
                byteIndex, bitMask, (long)item->Attributes.Logical.Minimum, (long)item->Attributes.Logical.Maximum);

        switch (item->Attributes.Usage.Page) {
        case HID_USAGE_PAGE_DESKTOP:
            switch (item->Attributes.Usage.Usage) {
            case HID_USAGE_DESKTOP_X:
                conn->xLoc = (pm_bt_field_loc_t){byteIndex, bitMask,
                    item->Attributes.Logical.Maximum, (int32_t)item->Attributes.Logical.Minimum};
                break;
            case HID_USAGE_DESKTOP_Y:
                conn->yLoc = (pm_bt_field_loc_t){byteIndex, bitMask,
                    item->Attributes.Logical.Maximum, (int32_t)item->Attributes.Logical.Minimum};
                break;
            case HID_USAGE_DESKTOP_Z:
                conn->zLoc = (pm_bt_field_loc_t){byteIndex, bitMask,
                    item->Attributes.Logical.Maximum, (int32_t)item->Attributes.Logical.Minimum};
                break;
            case HID_USAGE_DESKTOP_RZ:
                conn->rzLoc = (pm_bt_field_loc_t){byteIndex, bitMask,
                    item->Attributes.Logical.Maximum, (int32_t)item->Attributes.Logical.Minimum};
                break;
            case HID_USAGE_DESKTOP_HAT_SWITCH:
                conn->hatLoc.byteIndex = byteIndex;
                conn->hatLoc.bitMask = bitMask;
                break;
            default:
                break;
            }
            break;
        case HID_USAGE_PAGE_BUTTON: {
            uint8_t usage = item->Attributes.Usage.Usage;
            if (usage >= 1 && usage <= PM_BT_JOYPAD_MAX_BUTTONS) {
                conn->buttonLoc[usage - 1].byteIndex = byteIndex;
                conn->buttonLoc[usage - 1].bitMask = bitMask;
            }
            conn->buttonCnt++;
            break;
        }
        default:
            break;
        }
    }

    USB_FreeReportInfo(info);
    PM_INFO("[BT] Joypad descriptor parsed: %d buttons, axes x=%d y=%d z=%d rz=%d hat=%d\n",
            conn->buttonCnt, conn->xLoc.bitMask != 0, conn->yLoc.bitMask != 0,
            conn->zLoc.bitMask != 0, conn->rzLoc.bitMask != 0, conn->hatLoc.bitMask != 0);
}

// Reads a descriptor-located field out of a report and scales it to 0..255
// (centered at 128), matching the convention used by the USB generic
// gamepad driver's analog axes.
static uint8_t pm_bluetooth_scale_field(const pm_bt_field_loc_t *loc,
                                        const uint8_t *report, uint16_t report_len)
{
    if (!loc->bitMask || loc->byteIndex >= report_len) {
        return 128;
    }

    uint32_t raw;
    if (loc->bitMask > 0xFF && (uint16_t)(loc->byteIndex + 1) < report_len) {
        uint16_t combined = report[loc->byteIndex] | ((uint16_t)report[loc->byteIndex + 1] << 8);
        raw = (combined & loc->bitMask) >> __builtin_ctz(loc->bitMask);
    } else {
        raw = report[loc->byteIndex] & loc->bitMask;
    }

    int32_t span = (int32_t)loc->max - loc->min;
    if (span <= 0) {
        return 128;
    }
    int32_t centered = (int32_t)raw - loc->min;
    int32_t scaled = 1 + (centered * 254) / span;
    if (scaled < 1) scaled = 1;
    if (scaled > 255) scaled = 255;
    return (uint8_t)scaled;
}

static void pm_bluetooth_joypad_report(const pm_bluetooth_connection_t *conn,
                                       const uint8_t *report, uint16_t report_len)
{
    input_event_t event;
    uint32_t buttons = 0;
    // Reports can stream continuously; only log when the raw bytes change
    // so a live test session doesn't flood the console.
    static uint8_t last_report[32];
    static uint16_t last_report_len = 0xFFFF;
    bool changed = report_len != last_report_len ||
        memcmp(report, last_report, report_len > sizeof(last_report) ? sizeof(last_report) : report_len) != 0;

    if (changed) {
        pm_bluetooth_hex_dump("Joypad raw report", report, report_len);
        last_report_len = report_len;
        memcpy(last_report, report, report_len > sizeof(last_report) ? sizeof(last_report) : report_len);
    }

    init_input_event(&event);
    event.dev_addr = (uint8_t)conn->connection_handle;
    event.instance = 0;
    event.type = INPUT_TYPE_GAMEPAD;
    event.transport = INPUT_TRANSPORT_BT_BLE;

    for (uint8_t index = 0; index < PM_BT_JOYPAD_MAX_BUTTONS; ++index) {
        const pm_bt_field_loc_t *loc = &conn->buttonLoc[index];
        if (loc->bitMask && loc->byteIndex < report_len && (report[loc->byteIndex] & loc->bitMask)) {
            buttons |= (1u << index);
        }
    }

    if (conn->hatLoc.bitMask && conn->hatLoc.byteIndex < report_len) {
        uint8_t hat = report[conn->hatLoc.byteIndex] & conn->hatLoc.bitMask;

        event.hat[0] = hat <= 7 ? hat : 0xFF;
        switch (hat) {
        case 0: buttons |= JP_BUTTON_DU; break;
        case 1: buttons |= JP_BUTTON_DU | JP_BUTTON_DR; break;
        case 2: buttons |= JP_BUTTON_DR; break;
        case 3: buttons |= JP_BUTTON_DR | JP_BUTTON_DD; break;
        case 4: buttons |= JP_BUTTON_DD; break;
        case 5: buttons |= JP_BUTTON_DD | JP_BUTTON_DL; break;
        case 6: buttons |= JP_BUTTON_DL; break;
        case 7: buttons |= JP_BUTTON_DL | JP_BUTTON_DU; break;
        default: break;
        }
    }

    event.buttons = buttons;
    event.analog[ANALOG_LX] = pm_bluetooth_scale_field(&conn->xLoc, report, report_len);
    event.analog[ANALOG_LY] = pm_bluetooth_scale_field(&conn->yLoc, report, report_len);
    event.analog[ANALOG_RX] = pm_bluetooth_scale_field(&conn->zLoc, report, report_len);
    event.analog[ANALOG_RY] = pm_bluetooth_scale_field(&conn->rzLoc, report, report_len);

    if (changed) {
        PM_INFO("[BT] Joypad parsed: buttons=0x%03lX hat=%d LX=%u LY=%u RX=%u RY=%u\n",
                (unsigned long)buttons, event.hat[0], event.analog[ANALOG_LX],
                event.analog[ANALOG_LY], event.analog[ANALOG_RX], event.analog[ANALOG_RY]);
    }

    router_submit_input(&event);
}

static void pm_bluetooth_hids_handler(uint8_t packet_type, uint16_t channel,
                                      uint8_t *packet, uint16_t size)
{
    (void)packet_type;
    (void)channel;
    (void)size;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) {
        return;
    }

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
    case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED: {
        uint16_t cid = gattservice_subevent_hid_service_connected_get_hids_cid(packet);
        pm_bluetooth_slot_t slot = pm_bluetooth_slot_for_hids_cid(cid);

        if (slot >= PM_BT_SLOT_COUNT) break;

        if (gattservice_subevent_hid_service_connected_get_status(packet) == ERROR_CODE_SUCCESS) {
            bt_conn[slot].active = true;
            strcpy(bt_conn[slot].status, "Connected");
            if (slot == pending_slot) {
                bluetooth_state = PM_BT_WAITING;
                pending_slot = PM_BT_SLOT_COUNT;
                pairing_requested = false;
                selected_device_index = PM_BLUETOOTH_MAX_DEVICES;
                strcpy(scan_status, "Device connected");
            }
            PM_INFO("[BT] BLE %s ready\n", pm_bluetooth_slot_name(slot));

            if (slot == PM_BT_SLOT_JOYPAD) {
                const uint8_t *descriptor = hids_client_descriptor_storage_get_descriptor_data(cid, 0);
                uint16_t descriptor_len = hids_client_descriptor_storage_get_descriptor_len(cid, 0);

                PM_INFO("[BT] Joypad descriptor fetch: cid=%u ptr=%p len=%u\n",
                        cid, (void *)descriptor, descriptor_len);
                if (descriptor && descriptor_len) {
                    pm_bluetooth_joypad_parse_descriptor(&bt_conn[slot], descriptor, descriptor_len);
                } else {
                    PM_INFO("[BT] Joypad descriptor not available -- no reports will be decoded\n");
                }
            }
            pm_bluetooth_start_scan();
        } else {
            strcpy(bt_conn[slot].status, "Connection failed");
            PM_INFO("[BT] BLE %s connection failed\n", pm_bluetooth_slot_name(slot));
            pm_bluetooth_connection_reset(slot);
            if (slot == pending_slot) {
                pending_slot = PM_BT_SLOT_COUNT;
                pairing_requested = false;
                selected_device_index = PM_BLUETOOTH_MAX_DEVICES;
                strcpy(scan_status, "HID connection failed");
            }
            bluetooth_state = PM_BT_WAITING;
            pm_bluetooth_start_scan();
        }
        break;
    }
    case GATTSERVICE_SUBEVENT_HID_REPORT: {
        uint16_t cid = gattservice_subevent_hid_report_get_hids_cid(packet);
        pm_bluetooth_slot_t slot = pm_bluetooth_slot_for_hids_cid(cid);

        if (slot >= PM_BT_SLOT_COUNT || !bt_conn[slot].active) break;

        const uint8_t *report = gattservice_subevent_hid_report_get_report(packet);
        uint16_t report_len = gattservice_subevent_hid_report_get_report_len(packet);

        switch (slot) {
        case PM_BT_SLOT_MOUSE:
            pm_bluetooth_mouse_report(&bt_conn[slot], report, report_len);
            break;
        case PM_BT_SLOT_KEYBOARD:
            pm_bluetooth_keyboard_report(report, report_len);
            break;
        case PM_BT_SLOT_JOYPAD:
            pm_bluetooth_joypad_report(&bt_conn[slot], report, report_len);
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void pm_bluetooth_sm_handler(uint8_t packet_type, uint16_t channel,
                                    uint8_t *packet, uint16_t size)
{
    hci_con_handle_t handle = HCI_CON_HANDLE_INVALID;
    bool connect_hids = false;

    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST:
        sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        break;
    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        sm_numeric_comparison_confirm(sm_event_numeric_comparison_request_get_handle(packet));
        break;
    case SM_EVENT_PAIRING_COMPLETE:
        handle = sm_event_pairing_complete_get_handle(packet);
        connect_hids = sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS;
        break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
        handle = sm_event_reencryption_complete_get_handle(packet);
        connect_hids = true;
        break;
    default:
        break;
    }

    if (connect_hids && pending_slot < PM_BT_SLOT_COUNT &&
        bt_conn[pending_slot].connection_handle == handle) {
        pm_bluetooth_slot_t slot = pending_slot;
        // Mouse and keyboard use the standardized boot protocol; joypads
        // need Report protocol mode so BTstack discovers/parses the full
        // Report Map instead of just the boot input reports.
        hid_protocol_mode_t protocol_mode =
            (slot == PM_BT_SLOT_JOYPAD) ? HID_PROTOCOL_MODE_REPORT : HID_PROTOCOL_MODE_BOOT;

        PM_INFO("[BT] Pairing complete; connecting HID service (%s)\n", pm_bluetooth_slot_name(slot));
        bt_conn[slot].type = pm_bluetooth_get_device(selected_device_index)->type;
        hids_client_connect(handle, pm_bluetooth_hids_handler, protocol_mode, &bt_conn[slot].hids_cid);
    }
}

bool pm_bluetooth_scan_init(void)
{
    if (scanner_initialized) {
        PM_INFO("[BT] Scanner already running\n");
        return true;
    }

    PM_INFO("[BT] Initializing BLE scanner\n");
    if (!btstack_cyw43_init(cyw43_arch_async_context())) {
        PM_INFO("[BT] BTstack initialization failed\n");
        return false;
    }

    for (pm_bluetooth_slot_t slot = 0; slot < PM_BT_SLOT_COUNT; ++slot) {
        pm_bluetooth_connection_reset(slot);
    }

    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    gatt_client_init();
    hids_client_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));

    hci_event_callback_registration.callback = pm_bluetooth_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    sm_event_callback_registration.callback = pm_bluetooth_sm_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    hci_power_control(HCI_POWER_ON);
    scanner_initialized = true;
    PM_INFO("[BT] BLE scanner configured\n");
    return true;
}

bool pm_bluetooth_scan_start(void)
{
    if (!scanner_initialized || !btstack_working) {
        return false;
    }

    if (bluetooth_state == PM_BT_WAITING) {
        pm_bluetooth_start_scan();
    }
    return true;
}

bool pm_bluetooth_connect_device(uint8_t device_index)
{
    const pm_bluetooth_device_t *device;
    pm_bluetooth_slot_t slot;

    if (!scanner_initialized || !btstack_working || device_index >= pm_bluetooth_device_count()) {
        return false;
    }
    if (bluetooth_state != PM_BT_WAITING && bluetooth_state != PM_BT_SCANNING) {
        strcpy(scan_status, "Already pairing a device");
        return false;
    }

    device = pm_bluetooth_get_device(device_index);
    if (!pm_bluetooth_slot_for_type(device->type, &slot)) {
        strcpy(scan_status, "Selected device is not a mouse/keyboard/joypad");
        return false;
    }
    if (bt_conn[slot].active) {
        snprintf(scan_status, sizeof(scan_status), "%s slot already connected",
                 pm_bluetooth_slot_name(slot));
        return false;
    }

    pairing_requested = true;
    pending_slot = slot;
    selected_device_index = device_index;
    if (bluetooth_state == PM_BT_SCANNING) {
        gap_stop_scan();
    }
    pm_bluetooth_start_scan();
    PM_INFO("[BT] Pairing requested for device %d as %s\n", device_index + 1,
            pm_bluetooth_slot_name(slot));
    return true;
}

const char *pm_bluetooth_mouse_status(void)
{
    return scan_status;
}