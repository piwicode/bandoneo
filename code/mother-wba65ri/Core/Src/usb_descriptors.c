/*
 * usb_descriptors.c
 *
 *  Created on: Dec 28, 2025
 *      Author: pierre
 */

#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00, // Class defined at interface level
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xcafe, // Placeholder Vendor ID
    .idProduct          = 0x0001, // Placeholder Product ID
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,  // Index in string_desc_arr
    .iProduct           = 0x02,  // Index in string_desc_arr
    .iSerialNumber      = 0x03,  // Index in string_desc_arr

    .bNumConfigurations = 0x01
};

// Invoked when GET DEVICE DESCRIPTOR is received
uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum {
  ITF_NUM_MIDI = 0,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

uint8_t const desc_configuration[] = {
  // USB Configuration Descriptor.
  // Each configuration is a set of interface. There are not practical ways to
  // switch between the configurations, and it is unusual to have more than one.

  // TODO: implement BCD detection to identify charging-capable ports.
  TUD_CONFIG_DESCRIPTOR(
    1,             // config number (must be 1)
    ITF_NUM_TOTAL, // interface count
    0,             // string index
    CONFIG_TOTAL_LEN, // total length
    0x00,         // attribute
    500           // power in mA
  ),

  // MIDI Descriptor: 
  TUD_MIDI_DESCRIPTOR(
    ITF_NUM_MIDI, // interface number
    0,            // string index. It does not look to be used anywhere.
    0x01,         // Endpoint Out address
    0x81,         // Endpoint In address
    64            // Maximum packet size
  )
};

// Invoked when GET CONFIGURATION DESCRIPTOR is received
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
char const* string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 },    // 0: is supported language is English (0x0409).
  "L'Atelier du Bandoneon Libre",   // 1: Manufacturer
  "Bandoneo",             		    // 2: Product
  "BL-0001",                        // 3: Serials
};

static uint16_t _desc_str[32];

// Invoked when GET STRING DESCRIPTOR is received
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  uint8_t chr_count;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;
    const char* str = string_desc_arr[index];
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;
    for(uint8_t i=0; i<chr_count; i++) _desc_str[1+i] = str[i];
  }

  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
  return _desc_str;
}
