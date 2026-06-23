#include "usb_names.h"

#define MIDI_NAME     { 'O','P','L','2',' ','A','u','d','i','o',' ','B','o','a','r','d',' ','M','I','D','I' }
#define MIDI_NAME_LEN 21

struct usb_string_descriptor_struct usb_string_product_name = {
	2 + MIDI_NAME_LEN * 2,
	3,
	MIDI_NAME
};
