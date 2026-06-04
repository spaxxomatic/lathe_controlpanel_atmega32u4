/*
usb.h
*/
#ifndef USB_H
#define USB_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint8_t usb_config_status;

int usb_init();
bool get_usb_config_status();
int send_report(uint8_t buttons, int8_t delta);

#ifndef GET_STATUS
#define GET_STATUS 0x00
#endif
#ifndef CLEAR_FEATURE
#define CLEAR_FEATURE 0x01
#endif
#ifndef SET_FEATURE
#define SET_FEATURE 0x03
#endif
#ifndef SET_ADDRESS
#define SET_ADDRESS 0x05
#endif
#ifndef GET_DESCRIPTOR
#define GET_DESCRIPTOR 0x06
#endif
#ifndef GET_CONFIGURATION
#define GET_CONFIGURATION 0x08
#endif
#ifndef SET_CONFIGURATION
#define SET_CONFIGURATION 0x09
#endif
#ifndef GET_INTERFACE
#define GET_INTERFACE 0x0A
#endif
#ifndef SET_INTERFACE
#define SET_INTERFACE 0x0B
#endif

#define idVendor 0x03eb  // Atmel Corp.
#define idProduct 0x2ff4  // ATMega32u4 DFU Bootloader (This isn't a real product so I don't
          // have legitimate IDs)
#define KEYBOARD_ENDPOINT_NUM 3  // The second endpoint is the HID endpoint

#define CONFIG_SIZE 34
#define HID_OFFSET 18

// HID Class-specific request codes - refer to HID Class Specification
// Chapter 7.2 - Remarks

#define GET_REPORT 0x01
#define GET_IDLE 0x02
#define GET_PROTOCOL 0x03
#define SET_REPORT 0x09
#define SET_IDLE 0x0A
#define SET_PROTOCOL 0x0B

#ifdef __cplusplus
}
#endif

#endif
