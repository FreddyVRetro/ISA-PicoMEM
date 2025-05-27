
#include "xinput_host.h"
#include "../../pm_debug.h"


// Re define the fonctions for the xinput driver array
bool xinputh_init(void);
bool xinputh_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
bool xinputh_set_config(uint8_t dev_addr, uint8_t itf_num);
bool xinputh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void xinputh_close(uint8_t dev_addr);

usbh_class_driver_t usbh_drivers_array[2] = 
 {
    {.name      ="XINPUT",
    .init       = xinputh_init,
    .open       = xinputh_open,
    .set_config = xinputh_set_config,
    .xfer_cb    = xinputh_xfer_cb,
    .close      = xinputh_close},
   {.name = "MIDIH",
    .init=midih_init,
    .deinit=midih_deinit,
    .open=midih_open,
    .set_config=midih_set_config,
    .xfer_cb = midih_xfer_cb,
    .close = midih_close}
};

// Add all the USB Host custom drivers
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count){
    *driver_count = 1;
    PM_INFO("TUSB : Detect XInput \n");
    return &usbh_xinput_driver;
}