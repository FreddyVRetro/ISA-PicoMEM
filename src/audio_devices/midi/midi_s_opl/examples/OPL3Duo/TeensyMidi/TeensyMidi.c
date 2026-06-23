#include "usb_names.h"

#define MIDI_NAME     { 'O','P','L','3','D','u','o',' ','M','I','D','I' }
#define MIDI_NAME_LEN 12

struct usb_string_descriptor_struct usb_string_product_name = {
	2 + MIDI_NAME_LEN * 2,
	3,
	MIDI_NAME
};
