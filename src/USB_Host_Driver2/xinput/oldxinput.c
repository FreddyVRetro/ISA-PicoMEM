
/* #include "bsp/board_api.h" */
#include "tusb.h"
#include "xinput_host.h"
#include "usb.h"

#include "inputs/buttons.h"
#include "inputs/players/manager.h"
#include "inputs/players/feedback.h"
#include "inputs/router/router.h"
#include "inputs/input_event.h"

#include "../../pm_debug.h"
#include "hid_dev.h"

//Since https://github.com/hathach/tinyusb/pull/2222, we can add in custom vendor drivers easily
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count){
    *driver_count = 1;
    PM_INFO("TUSB : Detect XInput \n");
    return &usbh_xinput_driver;
}

uint8_t byteScaleAnalog(int16_t xbox_val);

/*
inline void update_joystate_xinput(uint16_t wButtons, int16_t sThumbLX, int16_t sThumbLY, int16_t sThumbRX, int16_t sThumbRY, uint8_t bLeftTrigger, uint8_t bRightTrigger) {
    uint8_t dpad = wButtons & 0xf;
    if (!dpad) {
        joystate_struct.joy1_x = ((int32_t)sThumbLX + 32768) >> 8;
        joystate_struct.joy1_y = ((-(int32_t)sThumbLY) + 32767) >> 8;
    } else {
        joystate_struct.joy1_x = (dpad & XINPUT_GAMEPAD_DPAD_RIGHT) ? 255 : ((dpad & XINPUT_GAMEPAD_DPAD_LEFT) ? 0 : 127);
        joystate_struct.joy1_y = (dpad & XINPUT_GAMEPAD_DPAD_DOWN) ? 255 : ((dpad & XINPUT_GAMEPAD_DPAD_UP) ? 0 : 127);
    }
    if (bLeftTrigger) {
        joystate_struct.joy1_y = 127u + (bLeftTrigger >> 1);
    } else if (bRightTrigger) {
        joystate_struct.joy1_y = 127u - (bRightTrigger >> 1);
    }
    joystate_struct.joy2_x = ((int32_t)sThumbRX + 32768) >> 8;
    joystate_struct.joy2_y = ((-(int32_t)sThumbRY) + 32767) >> 8;
    joystate_struct.button_mask = (~(wButtons >> 12)) << 4;
    //PM_INFO("%04x %04x\n", wButtons, joystate_struct.button_mask);
}
*/

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const* xid_itf, uint16_t len)
{
  uint32_t buttons;
  const xinput_gamepad_t *p = &xid_itf->pad;
  const char* type_str;

  if (xid_itf->last_xfer_result == XFER_RESULT_SUCCESS)
  {
    switch (xid_itf->type)
    {
      case 1: type_str = "Xbox One";          break;
      case 2: type_str = "Xbox 360 Wireless"; break;
      case 3: type_str = "Xbox 360 Wired";    break;
      case 4: type_str = "Xbox OG";           break;
      default: type_str = "Unknown";
    }

    if (xid_itf->connected && xid_itf->new_pad_data)
    {
      // TU_LOG1("[%02x, %02x], Type: %s, Buttons %04x, LT: %02x RT: %02x, LX: %d, LY: %d, RX: %d, RY: %d\n",
      //   dev_addr, instance, type_str, p->wButtons, p->bLeftTrigger, p->bRightTrigger, p->sThumbLX, p->sThumbLY, p->sThumbRX, p->sThumbRY);

      // Scale Xbox analog values to [1-255] range (platform-agnostic)
      // XInput uses positive Y = UP, but internal format uses Y: 0=UP, 255=DOWN
      // So we invert Y-axis values to match HID convention
      uint8_t analog_1x = byteScaleAnalog(p->sThumbLX);
      uint8_t analog_1y = 256 - byteScaleAnalog(p->sThumbLY);  // Invert Y
      uint8_t analog_2x = byteScaleAnalog(p->sThumbRX);
      uint8_t analog_2y = 256 - byteScaleAnalog(p->sThumbRY);  // Invert Y
      uint8_t analog_l = p->bLeftTrigger;
      uint8_t analog_r = p->bRightTrigger;

      buttons = (((p->wButtons & XINPUT_GAMEPAD_DPAD_UP)       ? JP_BUTTON_DU : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)      ? JP_BUTTON_DD : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)      ? JP_BUTTON_DL : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)     ? JP_BUTTON_DR : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_A)              ? JP_BUTTON_B1 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_B)              ? JP_BUTTON_B2 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_X)              ? JP_BUTTON_B3 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_Y)              ? JP_BUTTON_B4 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  ? JP_BUTTON_L1 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? JP_BUTTON_R1 : 0) |
                // Note: No threshold-based L2/R2 - let output profiles handle analog-to-digital
                // Xbox triggers are purely analog; digital behavior is profile/output dependent
                ((p->wButtons & XINPUT_GAMEPAD_BACK)           ? JP_BUTTON_S1 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_START)          ? JP_BUTTON_S2 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     ? JP_BUTTON_L3 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    ? JP_BUTTON_R3 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_GUIDE)          ? JP_BUTTON_A1 : 0) |
                ((p->wButtons & XINPUT_GAMEPAD_SHARE)          ? JP_BUTTON_A2 : 0));

      input_event_t event = {
        .dev_addr = dev_addr,
        .instance = instance,
        .type = INPUT_TYPE_GAMEPAD,
        .transport = INPUT_TRANSPORT_USB,
        .buttons = buttons,
        .button_count = 10,  // Xbox: A, B, X, Y, LB, RB, LT, RT, L3, R3
        .analog = {analog_1x, analog_1y, analog_2x, analog_2y, analog_l, analog_r},
        .keys = 0,
        .has_chatpad = false
//        .chatpad = {xid_itf->chatpad_data[0], xid_itf->chatpad_data[1], xid_itf->chatpad_data[2]},
//        .has_chatpad = xid_itf->chatpad_enabled && xid_itf->chatpad_inited
      };
      router_submit_input(&event);
    }
  }
  tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf)
{
    PM_INFO("XINPUT MOUNTED %02x %d\n", dev_addr, instance);

    uint16_t vid, pid;
    char VendorName[14]="";
    const char* type_str;

// Add the Model Type in the status string
    tuh_vid_pid_get(dev_addr, &vid, &pid);
  //  USB_GetVendorName(VendorName,vid,pid);

    switch (xinput_itf->type)
        {
            case 1: type_str = "Xbox One";          break;
            case 2: type_str = "Xbox 360 Wireless"; break;
            case 3: type_str = "Xbox 360 Wired";    break;
            case 4: type_str = "Xbox OG";           break;
            default: type_str = "Unknown";
        }
    int player_nb;
    player_nb=add_player(dev_addr, instance);
    usb_set_status(dev_addr,"%s %s Joystick (XInput)",VendorName,type_str);
    #if PM_PRINTF
    usb_print_status();
    #endif

    // If this is a Xbox 360 Wireless controller we need to wait for a connection packet
    // on the in pipe before setting LEDs etc. So just start getting data until a controller is connected.
    if (xinput_itf->type == XBOX360_WIRELESS && xinput_itf->connected == false) {
        tuh_xinput_receive_report(dev_addr, instance);
        return;
    } else if (xinput_itf->type == XBOX360_WIRED) {
        /*
         * Some third-party Xbox 360-style controllers require this message to finish initialization.
         * Idea taken from Linux drivers/input/joystick/xpad.c
         */
        uint8_t dummy[20];
        tusb_control_request_t const request =
        {
            .bmRequestType_bit =
            {
                .recipient = TUSB_REQ_RCPT_INTERFACE,
                .type      = TUSB_REQ_TYPE_VENDOR,
                .direction = TUSB_DIR_IN
            },
            .bRequest = tu_htole16(0x01),
            .wValue   = tu_htole16(0x100),
            .wIndex   = tu_htole16(0x00),
            .wLength  = 20
        };
        tuh_xfer_t xfer =
        {
            .daddr       = dev_addr,
            .ep_addr     = 0,
            .setup       = &request,
            .buffer      = dummy,
            .complete_cb = NULL,
            .user_data   = 0
        };
        tuh_control_xfer(&xfer);
    }

    if (player_nb!=-1) tuh_xinput_set_led(dev_addr, instance, player_nb+1, true);
    tuh_xinput_set_led(dev_addr, instance, 3, true);
    tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
    tuh_xinput_receive_report(dev_addr, instance);
}

uint8_t byteScaleAnalog(int16_t xbox_val)
{
  // Scale the xbox value from [-32768, 32767] to [1, 255]
  // Offset by 32768 to get in range [0, 65536], then divide by 256 to get in range [1, 255]
  uint8_t scale_val = (xbox_val + 32768) / 256;
  if (scale_val == 0) return 1;
  return scale_val;
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("XINPUT UNMOUNTED %02x %d\n", dev_addr, instance);
  
  remove_players_by_address(dev_addr, instance);

  // Unregister from auth passthrough
 // xbone_auth_unregister(dev_addr);
}

/*

void xinput_task(void)
{
  // Process Xbox One auth passthrough
 // xbone_auth_task();

  uint32_t now = to_ms_since_boot(get_absolute_time());

  // Chatpad keepalive for all xinput devices (runs even without player assignment)
  // TODO: chatpad keepalive disabled until chatpad support is working
  // for (uint8_t dev_addr = 1; dev_addr <= CFG_TUH_DEVICE_MAX; dev_addr++)
  // {
  //   for (uint8_t instance = 0; instance < CFG_TUH_XINPUT; instance++)
  //   {
  //     if (now - chatpad_last_keepalive[dev_addr][instance] >= XINPUT_CHATPAD_KEEPALIVE_MS)
  //     {
  //       if (tuh_xinput_chatpad_keepalive(dev_addr, instance))
  //       {
  //         chatpad_last_keepalive[dev_addr][instance] = now;
  //       }
  //     }
  //   }
  // }

  // Rumble/LED only if controller connected to a player
  if (!playersCount) return;

  // Throttle feedback to every 20ms (matches HID device task intervals)
  static uint32_t last_feedback_ms = 0;
  if (now - last_feedback_ms < 20) return;
  last_feedback_ms = now;

  // Track last-sent values per player to avoid redundant USB transfers
  static uint8_t last_led[4] = {0};
  static uint8_t last_rumble[4] = {0};

  // Update rumble/LED state for each xinput device
  for (int i = 0; i < playersCount; ++i)
  {
    if (players[i].dev_addr < 0) continue;  // Skip empty slots
    if (players[i].transport != INPUT_TRANSPORT_USB) continue;  // USB only

    uint8_t dev_addr = players[i].dev_addr;
    uint8_t instance = players[i].instance;

    // Get per-player feedback state
    feedback_state_t* fb = feedback_get_state(i);
    uint8_t rumble = fb ? (fb->rumble.left > fb->rumble.right ? fb->rumble.left : fb->rumble.right) : 0;

    // Derive LED quadrant from feedback pattern (set by host console)
    // Pattern bitmask: 0x01=P1, 0x02=P2, 0x04=P3, 0x08=P4
    // XInput quadrant: 1=P1, 2=P2, 3=P3, 4=P4, 0=off
    uint8_t led = i + 1;  // Default to player slot
    if (fb && fb->led.pattern) {
      if (fb->led.pattern & 0x01) led = 1;
      else if (fb->led.pattern & 0x02) led = 2;
      else if (fb->led.pattern & 0x04) led = 3;
      else if (fb->led.pattern & 0x08) led = 4;
    }

    // Only send if values changed (non-blocking)
    if (led != last_led[i]) {
      tuh_xinput_set_led(dev_addr, instance, led, false);
      last_led[i] = led;
    }
    if (rumble != last_rumble[i]) {
      tuh_xinput_set_rumble(dev_addr, instance, rumble, rumble, false);
      last_rumble[i] = rumble;
    }
  }
}
*/