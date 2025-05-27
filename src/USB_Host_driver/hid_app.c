/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
// hid_app for the ISA PicoMEM by FreddyV

#include "tusb.h"
#include "usb.h"
#include "hid_dev.h"
#include "../pm_debug.h"

pm_mouse_t pm_mouse;

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0
#define MAX_REPORT  4

#define HID_MAX_REPORT 4
static struct hid_info
{
    uint8_t report_count;
    tuh_hid_report_info_t report_info[HID_MAX_REPORT];
} hid_info[CFG_TUH_DEVICE_MAX][CFG_TUH_HID];

extern void process_kbd_report(hid_keyboard_report_t const *report);
extern void process_joy_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
static void process_mouse_report(hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

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
     case 0x146b : strcpy(VendorName,"BigBen ");
                 break;                  
     default : break;
    }
  PM_INFO("** GetVendorName: %s\n",VendorName);    
}

void hid_app_task(void)
{
  // nothing to do
}

//--------------------------------------------------------------------+
// TinyUSB "Generic" Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    struct hid_info dev_hid_info = hid_info[dev_addr][instance];
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_NONE)
    {
        dev_hid_info.report_count =
            tuh_hid_parse_report_descriptor(
                dev_hid_info.report_info,
                HID_MAX_REPORT, desc_report, desc_len);
    }

    bool has_keyboard = false;
    bool has_mouse = false;
    bool has_joystick = false;    
    uint8_t other_reports = 0;
  PM_INFO("** tuh_hid_mount_cb Begining:\n");
  PM_INFO("** HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

// Read Interfaces Protocol to detect KB, Mouse or None
    for (int i = 0; i <= instance; i++)
    {
        switch (tuh_hid_interface_protocol(dev_addr, i))
        {
        case HID_ITF_PROTOCOL_KEYBOARD:
            has_keyboard = true;
 //            kbd_hid_leds_dirty();
            break;
        case HID_ITF_PROTOCOL_MOUSE:
            has_mouse = true;
            break;
        case HID_ITF_PROTOCOL_NONE:
            other_reports++;
            break;
        }
    }

// Read Reports usage, to detect Joystick
     for (int i = 0; i < dev_hid_info.report_count; ++i) {
        PM_INFO("HID device report id: %u usage: %u usage page: %u\n",dev_hid_info.report_info[i].report_id, dev_hid_info.report_info[i].usage, dev_hid_info.report_info[i].usage_page);
        if (dev_hid_info.report_info[i].usage == HID_USAGE_DESKTOP_JOYSTICK || dev_hid_info.report_info[i].usage == HID_USAGE_DESKTOP_GAMEPAD) 
        {
            has_joystick = true;
            PM_INFO(" * joystick detected\n");
        }
    }

  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  char VendorName[14]="";

  USB_GetVendorName(VendorName,vid,pid);

PM_INFO (" - V:%s M:%d K:%d J:%d R:%d",VendorName,has_mouse,has_keyboard,has_joystick,other_reports);
/*    if (has_keyboard && has_mouse && other_reports)
        usb_set_status(dev_addr,"%sUSB keyboard, mouse, and %d other reports",VendorName, other_reports);
    else if (has_keyboard && other_reports)
        usb_set_status(dev_addr,"%sUSB keyboard and %d other reports",VendorName, other_reports);
    else if (has_mouse && other_reports)
        usb_set_status(dev_addr,"%sUSB mouse and %d other reports",VendorName, other_reports);        
    else 
*/
    if (has_keyboard && has_mouse)
        usb_set_status(dev_addr,"%sUSB keyboard and mouse",VendorName);
    else if (has_keyboard)
        usb_set_status(dev_addr,"%sUSB keyboard",VendorName);
    else if (has_mouse)
        usb_set_status(dev_addr,"%sUSB mouse",VendorName);
    else if (has_joystick)
        usb_set_status(dev_addr,"%sUSB joystick",VendorName);
/*    else
        usb_set_status(dev_addr, "%sUSB %d report%s v:%x p:%x",VendorName, other_reports, other_reports == 1 ? "" : "s",vid,pid);
*/        
  
  usb_print_status();

  // request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    PM_INFO("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  PM_INFO("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  PM_INFO("tuh_hid_report_received_cb\n");
  
  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report( (hid_keyboard_report_t const*) report );
      break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report( (hid_mouse_report_t const*) report );
      break;

    default:
      // Generic report requires matching ReportID and contents with previous parsed report info
      process_joy_report(dev_addr, instance, report, len);
 //     process_generic_report(dev_addr, instance, report, len);
      break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    PM_INFO("Error: cannot request to receive report\r\n");
  }
}



//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

static void process_mouse_report(hid_mouse_report_t const * report)
{
  static hid_mouse_report_t prev_report = { 0 };

#if PM_PRINTF
  //------------- button state  -------------//
  uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  if ( button_changed_mask & report->buttons)
  {
    printf(" %c%c%c ",
       report->buttons & MOUSE_BUTTON_LEFT   ? 'L' : '-',
       report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-',
       report->buttons & MOUSE_BUTTON_RIGHT  ? 'R' : '-');
    //pm_mouse.event=true;     
  }
#endif

  //------------- cursor movement -------------//
  //mouse_cursor_movement(report->x, report->y, report->wheel);

  // Compute the delta with a Divisor, to adjust the Speed
  // Declare a mouse event only if the result value change to reduce the number of IRQ going to the PC
 
  pm_mouse.buttons=report->buttons;
//  pm_mouse.x+=report->x;
//  pm_mouse.y+=report->y;
  pm_mouse.x=report->x;
  pm_mouse.y=report->y;
  pm_mouse.event=true;
  
  /*
  pm_mouse.x=(int8_t)(pm_mouse.xl*2/pm_mouse.div);
  pm_mouse.y=(int8_t)(pm_mouse.yl*2/pm_mouse.div);
  if ((pm_mouse.x!=pm_mouse.prevx) || (pm_mouse.y!=pm_mouse.prevy)) 
     pm_mouse.event=true;
  */
#if PM_PRINTF
  printf("(%d %d w:%d)\r\n", pm_mouse.x ,pm_mouse.y,report->wheel);
#endif  
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = hid_info[dev_addr][instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = hid_info[dev_addr][instance].report_info;
  tuh_hid_report_info_t* rpt_info = NULL;

  if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
  {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  }else
  {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the array
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
    printf("Couldn't find the report info for this report !\r\n");
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
        process_kbd_report( (hid_keyboard_report_t const*) report );
      break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        process_mouse_report( (hid_mouse_report_t const*) report );
      break;

      default: 
         printf("Other report\n");
       break;
    }
  }
}
