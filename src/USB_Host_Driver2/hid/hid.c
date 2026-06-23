// hid.c - HID protocol handler (TinyUSB HID host callbacks)
#include "tusb.h"
#include "inputs/buttons.h"
//#include "outputs/output_interface.h"
#include "inputs/players/manager.h"
#include "inputs/players/feedback.h"
//#include "inputs/services/profiles/profile_indicator.h"
//#include "inputs/services/codes/codes.h"
#include "hid_utils.h"
#include "hid_registry.h"
//#include "/hid/devices/vendors/sony/sony_ds4.h"
//#include "/hid/vendors/sony/sony_ds4.h"
#include "usb.h"
#include "../../../pm_debug.h"


// Auth task - called from main loop to handle async operations
void ds4_auth_task(void);

// Register/unregister DS4 for auth passthrough (called on mount/unmount)
void ds4_auth_register(uint8_t dev_addr, uint8_t instance);
void ds4_auth_unregister(uint8_t dev_addr, uint8_t instance);



// #define LANGUAGE_ID 0x0409
#define MAX_REPORTS 5

// Each HID instance can have multiple reports
typedef struct TU_ATTR_PACKED
{
  dev_type_t type;
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORTS];
} instance_t;

// Cached device report properties on mount
typedef struct TU_ATTR_PACKED
{
  uint16_t vid, pid;
  instance_t instances[CFG_TUH_HID];
  // uint16_t serial[20];
} device_t;

static device_t devices[MAX_DEVICES] = { 0 };
int16_t spinner = 0;

static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

void hid_init()
{
  register_devices();

  for(uint8_t dev_addr=1; dev_addr<MAX_DEVICES; dev_addr++)
  {
    for(uint8_t instance=0; instance<CFG_TUH_HID; instance++)
    {
     devices[dev_addr].instances[instance].type= CONTROLLER_UNKNOWN;
     devices[dev_addr].instances[instance].report_count=0;
    }
  }
}

void hid_task(void)
{
  // Process DS4 auth passthrough
  ds4_auth_task();

  // Get test mode counter (for LED test patterns)
  // uint8_t test_counter = codes_get_test_counter();
  uint8_t test_counter = 0;
  
  // Iterate devices and instances that can receive responses
  for(uint8_t dev_addr=1; dev_addr<MAX_DEVICES; dev_addr++)
  {
    for(uint8_t instance=0; instance<CFG_TUH_HID; instance++)
    {
      if (devices[dev_addr].instances[instance].type==-1) continue;
      PM_INFO("HID TASK: Checking DEV %d|%d type %d\r\n", dev_addr, instance, devices[dev_addr].instances[instance].type);

      int8_t player_index = find_player_index(dev_addr, instance);
      int8_t ctrl_type = devices[dev_addr].instances[instance].type;
      switch (ctrl_type)
      {
      case CONTROLLER_DUALSENSE:  // send DS5 LED and rumble
      case CONTROLLER_DUALSHOCK3: // send DS3 Init, LED and rumble
      case CONTROLLER_DUALSHOCK4: // send DS4 LED and rumble
      case CONTROLLER_GAMECUBE:   // send GameCube WiiU/Switch Adapter rumble
      case CONTROLLER_KEYBOARD:   // send Keyboard LEDs
      case CONTROLLER_SWITCH:     // send Switch Pro init, LED and rumble commands
      case CONTROLLER_SWITCH2:    // send Switch 2 Pro init, LED and rumble commands
      case CONTROLLER_SINPUT:     // send SInput rumble, player LED, and RGB LED
      case CONTROLLER_SIDEWINDER_COMMANDER: // send Strategic Commander LEDs
        {
          // Get per-player feedback state
          feedback_state_t* fb = (player_index >= 0) ? feedback_get_state(player_index) : NULL;

          // Derive player LED index from feedback pattern (for USB output passthrough)
          // Supports combo patterns for players 5-7 via PLAYER_LEDS[] lookup
          int8_t led_player_index = -1;
          if (fb && fb->led.pattern) {
            for (int p = 1; p <= 7; p++) {
              if (fb->led.pattern == PLAYER_LEDS[p]) {
                led_player_index = p - 1;
                break;
              }
            }
          }

          // Use feedback LED player if set, otherwise use profile indicator display
          //int8_t display_player_index = (led_player_index >= 0) ? player_index : 0;
          int8_t display_player_index = player_index;
//          PM_INFO("display_player_index %d led_player_index %d player_index %d\r\n", display_player_index, led_player_index, player_index);

          
          // Get trigger threshold from output interface (profile-based adaptive triggers)
          uint8_t trigger_threshold = 0;
  /*        if (active_output && active_output->get_trigger_threshold) {
            trigger_threshold = active_output->get_trigger_threshold();
            }
 */

          // Build legacy device output configuration from feedback state
          device_output_config_t config = {
            .player_index = display_player_index,
            .rumble = fb ? (fb->rumble.left > fb->rumble.right ? fb->rumble.left : fb->rumble.right) : 0,
            .rumble_left = fb ? fb->rumble.left : 0,
            .rumble_right = fb ? fb->rumble.right : 0,
            .leds = fb ? fb->led.pattern : 0,
            .trigger_threshold = trigger_threshold,
            .test = test_counter
          };

         PM_INFO("HID TASK: DEV %d|%d, TYPE %d, PLAYER_IDX %d, RUMBLE L%d R%d, LED PATTERN 0x%02x\r\n",
                  dev_addr, instance, ctrl_type, display_player_index,
                  config.rumble_left, config.rumble_right, config.leds);

          device_interfaces[ctrl_type]->task(dev_addr, instance, &config);
        }
        break;
      default:
        break;
      }
    }
  }

}

void USB_GetVendorName(char *VendorName,uint16_t vid,uint16_t pid)
{
// https://cateee.net/lkddb/web-lkddb/INPUT_JOYSTICK.html

  PM_INFO("** Vendor: %x Product: %x \n",vid,pid);
   switch (vid)
    {
     case 0x054c: strcpy(VendorName,"Sony ");                 
                 break;
     case 0x046D: strcpy(VendorName,"Logitech ");
                 break;     
     case 0x03F0: strcpy(VendorName,"HP ");   
                 break;  
     case 0x17EF: strcpy(VendorName,"Lenovo ");
                 break;                  
     case 0x413C: strcpy(VendorName,"Dell ");
                 break;
     case 0x045e: strcpy(VendorName,"Microsoft ");    
                 break;   
     case 0x057e: strcpy(VendorName,"Nintendo ");
                 break; 
     case 0x044F: strcpy(VendorName,"Thrustmaster ");
                 break;  
     case 0x0738: strcpy(VendorName,"Mad Catz ");
                 break;
     case 0x06A3: strcpy(VendorName,"Saitek ");
                 break;
     case 0x2dc8 : strcpy(VendorName,"8BitDo ");
                 break;
     case 0x146b : strcpy(VendorName,"Bigben ");
                 break;                  
     case 0x2563 : strcpy(VendorName,"G-LAB ");
                 break;
     case 0x0f0d : strcpy(VendorName,"HORI ");                
                  break;
     case 0x1292 : strcpy(VendorName,"Fire ");
                  break;
     case 0x2c22 : strcpy(VendorName,"Qanba ");
                  break;
                   
     default : 
                 break;
    }
  PM_INFO("** GetVendorName: %s\n",VendorName);    
}

dev_type_t get_dev_type(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  PM_INFO("VID = %04x, PID = %04x\r\n", vid, pid);

  for (int i = 0; i < CONTROLLER_TYPE_COUNT-2; i++) {
    if (device_interfaces[i] &&
        device_interfaces[i]->is_device(vid, pid)) {
      PM_INFO("DEVICE:[%s]\n", device_interfaces[i]->name);
      return (dev_type_t)i;
    }
  }

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  PM_INFO("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  switch (itf_protocol)
  {
  case HID_ITF_PROTOCOL_KEYBOARD:
    PM_INFO("DEVICE:[KEYBOARD]\n");
    return CONTROLLER_KEYBOARD;
    break;
  case HID_ITF_PROTOCOL_MOUSE:
    PM_INFO("DEVICE:[MOUSE]\n");
    return CONTROLLER_MOUSE;
    break;
  default:
    break;
  }

  if (device_interfaces[CONTROLLER_DINPUT]->check_descriptor(dev_addr, instance, desc_report, desc_len))
  {
    PM_INFO("DEVICE:[%s]\n", device_interfaces[CONTROLLER_DINPUT]->name);
    return CONTROLLER_DINPUT;
  }

  PM_INFO("DEVICE:[UNKNOWN]\n");
  return CONTROLLER_UNKNOWN;
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  PM_INFO("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  uint16_t vid, pid;
  char VendorName[14]="";

  dev_type_t dev_type = get_dev_type(dev_addr, instance, desc_report, desc_len);
  devices[dev_addr].instances[instance].type = dev_type;

  // Set device type and defaults
  switch (dev_type)
  {
  case CONTROLLER_DUALSHOCK3:
  case CONTROLLER_SWITCH:
  case CONTROLLER_SWITCH2:
  case CONTROLLER_SINPUT:
  case CONTROLLER_SIDEWINDER_COMMANDER:
    device_interfaces[dev_type]->init(dev_addr, instance);
    break;
  case CONTROLLER_DUALSHOCK4:
    // Register DS4 for auth passthrough
    ds4_auth_register(dev_addr, instance);
    break;
  default:
    break;
  }

  if ((dev_type!=CONTROLLER_KEYBOARD)&&(dev_type!=CONTROLLER_MOUSE)&&(dev_type!=CONTROLLER_UNKNOWN))
     {
      add_player(dev_addr, instance);
     }

  if (dev_type == CONTROLLER_UNKNOWN)
  {
    // Safe on ARM Cortex-M0+: suppress alignment warning for packed struct access
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    tuh_hid_report_info_t* report_info_ptr = devices[dev_addr].instances[instance].report_info;
    devices[dev_addr].instances[instance].report_count =
      tuh_hid_parse_report_descriptor(report_info_ptr, MAX_REPORTS, desc_report, desc_len);
    #pragma GCC diagnostic pop
    PM_INFO("HID has %u reports \r\n", devices[dev_addr].instances[instance].report_count);
  }

  tuh_vid_pid_get(dev_addr, &vid, &pid);
  USB_GetVendorName(VendorName,vid,pid);
  usb_set_status(dev_addr,"%s%s",VendorName,device_interfaces[dev_type]->name);
  #if PM_PRINTF
  usb_print_status();
  #endif

  // gets serial for discovering some devices
  // uint16_t temp_buf[128];
  // if (0 == tuh_descriptor_get_serial_string_sync(dev_addr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)))
  // {
  //   for(int i=0; i<20; i++){
  //     devices[dev_addr].serial[i] = temp_buf[i];
  //   }
  // }

  // request to receive report
  //tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    usb_set_status(dev_addr, "?HID cannot request report device %d instance %d", dev_addr, instance);
    PM_ERROR("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  PM_INFO("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);

  // Reset device states
  dev_type_t dev_type = devices[dev_addr].instances[instance].type;
  switch (dev_type)
  {
  case CONTROLLER_DINPUT:
  case CONTROLLER_DUALSENSE:
  case CONTROLLER_DUALSHOCK3:
  case CONTROLLER_DUALSHOCK4:
    device_interfaces[dev_type]->unmount(dev_addr, instance);
    // Unregister DS4 from auth passthrough
    if (dev_type == CONTROLLER_DUALSHOCK4) {
      ds4_auth_unregister(dev_addr, instance);
    }
    break;
  case CONTROLLER_KEYBOARD:
  case CONTROLLER_SWITCH:
  case CONTROLLER_SWITCH2:
  case CONTROLLER_SINPUT:
    device_interfaces[dev_type]->unmount(dev_addr, instance);
    break;
  default:
    break;
  }

  remove_players_by_address(dev_addr, instance);  
  devices[dev_addr].instances[instance].type = CONTROLLER_UNKNOWN;
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  //PM_INFO("HID device address = %d, instance = %d, received report\r\n", dev_addr, instance);

  dev_type_t dev_type = devices[dev_addr].instances[instance].type;
  if (dev_type == CONTROLLER_UNKNOWN)
  {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    switch (itf_protocol)
    {
      case HID_ITF_PROTOCOL_KEYBOARD:
        PM_INFO("HID receive boot keyboard report\r\n");
        device_interfaces[CONTROLLER_KEYBOARD]->process(dev_addr, instance, report, len);
      break;

      case HID_ITF_PROTOCOL_MOUSE:
        PM_INFO("HID receive boot mouse report\r\n");
        device_interfaces[CONTROLLER_MOUSE]->process(dev_addr, instance, report, len);
      break;

      default:
        PM_INFO("HID receive generic report\r\n");
        process_generic_report(dev_addr, instance, report, len);
      break;
    }
  }
  else
  {
    // process known device interface reports
    device_interfaces[dev_type]->process(dev_addr, instance, report, len);
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    PM_ERROR("Error: cannot request to receive report\r\n");
  }
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+

static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = devices[dev_addr].instances[instance].report_count;
  // Safe on ARM Cortex-M0+: suppress alignment warning for packed struct access
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
  tuh_hid_report_info_t* rpt_info_arr = devices[dev_addr].instances[instance].report_info;
  #pragma GCC diagnostic pop
  tuh_hid_report_info_t* rpt_info = NULL;

  if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
  {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  }
  else
  {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the arrray
    for(uint8_t i=0; i<rpt_count; i++)
    {
      if (rpt_id == rpt_info_arr[i].report_id )
      {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info)
  {
    PM_ERROR("Couldn't find the report info for this report !\r\n");
    return;
  }

  // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
  // - Keyboard                     : Desktop, Keyboard
  // - Mouse                        : Desktop, Mouse
  // - Gamepad                      : Desktop, Gamepad
  // - Consumer Control (Media Key) : Consumer, Consumer Control
  // - System Control (Power key)   : Desktop, System Control
  // - Generic (vendor)             : 0xFFxx, xx
  if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
  {
    switch (rpt_info->usage)
    {
      case HID_USAGE_DESKTOP_KEYBOARD:
        TU_LOG1("HID receive keyboard report\r\n");
        // Assume keyboard follow boot report layout
        device_interfaces[CONTROLLER_KEYBOARD]->process(dev_addr, instance, report, len);
      break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        device_interfaces[CONTROLLER_MOUSE]->process(dev_addr, instance, report, len);
      break;

      default: break;
    }
  }
}

// Get controller type for a device instance
// Returns -1 if invalid, otherwise returns dev_type_t enum value
int hid_get_ctrl_type(uint8_t dev_addr, uint8_t instance)
{
  if (dev_addr >= MAX_DEVICES || instance >= CFG_TUH_HID) {
    return -1;
  }
  return devices[dev_addr].instances[instance].type;
}
