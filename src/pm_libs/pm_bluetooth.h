#ifndef PM_BLUETOOTH_H
#define PM_BLUETOOTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PM_BLUETOOTH_MAX_DEVICES 12
#define PM_BLUETOOTH_NAME_LEN 21

typedef enum {
  PM_BLUETOOTH_DEVICE_UNKNOWN,
  PM_BLUETOOTH_DEVICE_MOUSE,
  PM_BLUETOOTH_DEVICE_KEYBOARD,
  PM_BLUETOOTH_DEVICE_JOYPAD,
  PM_BLUETOOTH_DEVICE_HEADPHONES,
  PM_BLUETOOTH_DEVICE_SPEAKER,
  PM_BLUETOOTH_DEVICE_PHONE,
  PM_BLUETOOTH_DEVICE_COMPUTER,
  PM_BLUETOOTH_DEVICE_WATCH,
  PM_BLUETOOTH_DEVICE_HEART_RATE,
  PM_BLUETOOTH_DEVICE_THERMOMETER,
  PM_BLUETOOTH_DEVICE_HID,
} pm_bluetooth_device_type_t;

typedef struct {
  uint8_t address[6];
  char name[PM_BLUETOOTH_NAME_LEN];
  int8_t rssi;
  uint8_t address_type;
  pm_bluetooth_device_type_t type;
  uint16_t appearance;
  uint8_t hid_service;
  uint8_t advertisement_length;
} pm_bluetooth_device_t;

void pm_bluetooth_record_device(const uint8_t address[6], const char *name, int8_t rssi);
void pm_bluetooth_record_device_info(const uint8_t address[6], const char *name, int8_t rssi,
                                     uint8_t address_type, pm_bluetooth_device_type_t type,
                                     uint16_t appearance, uint8_t hid_service,
                                     uint8_t advertisement_length);
uint8_t pm_bluetooth_device_count(void);
const pm_bluetooth_device_t *pm_bluetooth_get_device(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif