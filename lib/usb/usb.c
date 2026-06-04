/*
usb.c
USB Controller initialization, device setup, and HID interrupt routines
*/

#define F_CPU 16000000
#include "usb.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "avr/io.h"


static uint8_t this_interrupt = 0;

/*  Device Descriptor - The top level descriptor when enumerating a USB device`
        Specification: USB 2.0 (April 27, 2000) Chapter 9 Table 9-5

*/

static const uint8_t device_descriptor[] PROGMEM = {
    // Stored in PROGMEM (Program Memory) Flash, freeing up some SRAM where
    // variables are usually stored
    18,  // bLength - The total size of the descriptor
    1,   // bDescriptorType - The type of descriptor - 1 is device
    0x00,
    0x02,  // bcdUSB - The USB protcol supported - Refer to USB 2.0
           // Chapter 9.6.1
    0,   // bDeviceClass - The Device Class, 0 indicating that the HID interface
         // will specify it
    0,   // bDeviceSubClass - 0, HID will specify
    0,   // bDeviceProtocol - No class specific protocols on a device level, HID
         // interface will specify
    32,  // bMaxPacketSize0 - 32 byte packet size; control endpoint was
         // configured in UECFG1X to be 32 bytes
    (idVendor & 255),
    ((idVendor >> 8) &
     255),  // idVendor - Vendor ID specified by USB-IF (To fit the 2 bytes, the
            // ID is split into least significant and most significant byte)
    (idProduct & 255),
    ((idProduct >> 8) & 255),  // idProduct - The Product ID specified by USB-IF
                               // - Split in the same way as idVendor
    0x00,
    0x01,  // bcdDevice - Device Version Number
    0,  // iManufacturer - The String Descriptor that has the manufacturer name
        // -
        // Specified by USB 2.0 Table 9-8
    0,  // iProduct - The String Descriptor that has the product name -
        // Specified
        // by USB 2.0 Table 9-8
    0,  // iSerialNumber - The String Descriptor that has the serial number of
        // the product - Specified by USB 2.0 Table 9-8
    1   // bNumConfigurations - The number of configurations of the device, most
        // devices only have one
};

// HID report descriptor: Joystick with 4 buttons + 1 relative Dial axis
// Report layout (2 bytes):
//   Byte 0: bits 0-3 = buttons 1-4, bits 4-7 = padding
//   Byte 1: dial delta (int8, relative)
static const uint8_t dial_HID_descriptor[] PROGMEM = {
    0x05, 0x01,  // Usage Page: Generic Desktop
    0x09, 0x04,  // Usage: Joystick
    0xA1, 0x01,  // Collection: Application

    // 4 buttons
    0x05, 0x09,  //   Usage Page: Button
    0x19, 0x01,  //   Usage Minimum: Button 1
    0x29, 0x04,  //   Usage Maximum: Button 4
    0x15, 0x00,  //   Logical Minimum: 0
    0x25, 0x01,  //   Logical Maximum: 1
    0x75, 0x01,  //   Report Size: 1 bit
    0x95, 0x04,  //   Report Count: 4
    0x81, 0x02,  //   Input: Data, Variable, Absolute

    // 4 padding bits to complete the byte
    0x75, 0x01,  //   Report Size: 1 bit
    0x95, 0x04,  //   Report Count: 4
    0x81, 0x01,  //   Input: Constant

    // Dial axis
    0x05, 0x01,  //   Usage Page: Generic Desktop
    0x09, 0x37,  //   Usage: Dial
    0x15, 0x80,  //   Logical Minimum: -128
    0x25, 0x7F,  //   Logical Maximum: 127
    0x75, 0x08,  //   Report Size: 8 bits
    0x95, 0x01,  //   Report Count: 1
    0x81, 0x06,  //   Input: Data, Variable, Relative

    0xC0         // End Collection
};

/*  Configuration Descriptor - The descriptor that gives information about the
   device conifguration(s) and how to select them Specification: USB 2.0 (April
   27, 2000) Chapter 9 Table 9-10 Note: The reason the descriptors are
   structured the way they are is to better show the tree structure of the
   descriptors; if the HID inteface descriptor was separate from the
   configuration descriptor, we wouldn't need the HID_OFFSET, but that would
   imply that the descriptors are separate entities when they are really
   dependent on each other
*/

static const uint8_t configuration_descriptor[] PROGMEM = {
    9,  // bLength
    2,  // bDescriptorType - 2 is device
    (CONFIG_SIZE & 255),
    ((CONFIG_SIZE >> 8) &
     255),  // wTotalLength - The total length of the descriptor tree
    1,      // bNumInterfaces - 1 Interface
    1,      // bConfigurationValue
    0,      // iConfiguration - We have no string descriptors
    0xC0,   // bmAttributes - Set the device power source
    150,    // bMaxPower - 150 x 2mA units = 300mA max power consumption
    // Refer to Table 9-10 for the descriptor structure - Configuration
    // Descriptors have interface descriptors, interface descriptors have
    // endpoint descriptors along with a special HID descriptor
    9,     // bLength
    4,     // bDescriptorType - interface
    0,     // bInterfaceNumber
    0,     // bAlternateSetting
    1,     // bNumEndpoints
    0x03,  // bInterfaceClass - HID
    0x00,  // bInterfaceSubClass - 0 = no boot subclass
    0x00,  // bInterfaceProtocol - 0 = none
    0,     // iInterface
    // HID Descriptor
    9,           // bLength
    0x21,        // bDescriptorType - HID
    0x11, 0x01,  // bcdHID 1.11
    0,           // bCountryCode
    1,           // bNumDescriptors
    0x22,        // bDescriptorType - Report
    sizeof(dial_HID_descriptor), 0,  // wDescriptorLength
    // Endpoint Descriptor - Example can be found in the HID spec table E.5
    7,     // bLength
    0x05,  // bDescriptorType
    KEYBOARD_ENDPOINT_NUM |
        0x80,  // Set keyboard endpoint to IN endpoint, refer to table
    0x03,      // bmAttributes - Set endpoint to interrupt
    8, 0,      // wMaxPacketSize - The size of the keyboard banks
    0x01       // wInterval - Poll for new data 1000/s, or once every ms
};

int usb_init() {
  cli();  // Global Interrupt Disable

  UHWCON |= (1 << UVREGE);  // Enable USB Pads Regulator

  PLLCSR |= 0x12;  // Configure to use 16mHz oscillator

  while (!(PLLCSR & (1 << PLOCK)))
    ;  // Wait for PLL Lock to be achieved

  USBCON |=
      (1 << USBE) | (1 << OTGPADE);  // Enable USB Controller and USB power pads
  USBCON &= ~(1 << FRZCLK);          // Unfreeze the clock

  UDCON &= ~(1<<LSM);  // FULL SPEED MODE
  UDCON &= ~(1<<DETACH);  // Attach USB Controller to the data bus

  UDIEN |= (1 << EORSTE) |
           (1 << SOFE);  // Re-enable the EORSTE (End Of Reset) Interrupt so we
                         // know when we can configure the control endpoint
  usb_config_status = 0;
  sei();  // Global Interrupt Enable
  return 0;
}

int send_report(uint8_t buttons, int8_t delta) {
  if (!usb_config_status)
    return -1;
  cli();
  UENUM = KEYBOARD_ENDPOINT_NUM;
  while (!(UEINTX & (1 << RWAL)))
    ;  // wait for endpoint bank to be ready
  UEDATX = buttons & 0x0F;   // low 4 bits = buttons 1-4
  UEDATX = (uint8_t)delta;
  UEINTX = 0b00111010;
  sei();
  return 0;
}

bool get_usb_config_status() {
  return usb_config_status;
}

ISR(USB_GEN_vect) {
  uint8_t udint_temp = UDINT;
  UDINT = 0;

  if (udint_temp & (1 << EORSTI)) {  // If end of reset interrupt
    // Configure Control Endpoint
    UENUM = 0;             // Select Endpoint 0, the default control endpoint
    UECONX = (1 << EPEN);  // Enable the Endpoint
    UECFG0X = 0;      // Control Endpoint, OUT direction for control endpoint
    UECFG1X |= 0x22;  // 32 byte endpoint, 1 bank, allocate the memory
    usb_config_status = 0;

    if (!(UESTA0X &
          (1 << CFGOK))) {  // Check if endpoint configuration was successful
      return;
    }

    UERST = 1;  // Reset Endpoint
    UERST = 0;

    UEIENX =
        (1 << RXSTPE);  // Re-enable the RXSPTE (Receive Setup Packet) Interrupt
    return;
  }
  (void)this_interrupt;  // SOF interrupt not used for dial device
}

ISR(USB_COM_vect) {
  UENUM = 0;
  if (UEINTX & (1 << RXSTPI)) {
    uint8_t bmRequestType = UEDATX;  // UEDATX is FIFO; see table in README
    uint8_t bRequest = UEDATX;
    uint16_t wValue = UEDATX;
    wValue |= UEDATX << 8;
    uint16_t wIndex = UEDATX;
    wIndex |= UEDATX << 8;
    uint16_t wLength = UEDATX;
    wLength |= UEDATX << 8;

    DDRC = 0xFF;

    UEINTX &= ~(
        (1 << RXSTPI) | (1 << RXOUTI) |
        (1 << TXINI));  // Handshake the Interrupts, do this after recording
                        // the packet because it also clears the endpoint banks
    if (bRequest == GET_DESCRIPTOR) {
      // The Host is requesting a descriptor to enumerate the device
      uint8_t* descriptor;
      uint8_t descriptor_length;

      if (wValue == 0x0100) {  // Is the host requesting a device descriptor?
        descriptor = device_descriptor;
        descriptor_length = pgm_read_byte(descriptor);
      } else if (wValue ==
                 0x0200) {  // Is it asking for a configuration descriptor?
        descriptor = configuration_descriptor;
        descriptor_length =
            CONFIG_SIZE;  // Configuration descriptor is comprised of many
                          // different descriptors; the length is more than
                          // bLength
      } else if (wValue ==
                 0x2100) {  // Is it asking for a HID Report Descriptor?
        descriptor = configuration_descriptor + HID_OFFSET;
        descriptor_length = pgm_read_byte(descriptor);
      } else if (wValue == 0x2200) {
        descriptor = dial_HID_descriptor;
        descriptor_length = sizeof(dial_HID_descriptor);
      } else {
        PORTC = 0xFF;
        UECONX |=
            (1 << STALLRQ) | (1 << EPEN);  // Enable the endpoint and stall, the
                                           // descriptor does not exist
        return;
      }

      uint8_t request_length =
          wLength > 255 ? 255
                        : wLength;  // Our endpoint is only so big; the USB Spec
                                    // says to truncate the response if the size
                                    // exceeds the size of the endpoint

      descriptor_length =
          request_length > descriptor_length
              ? descriptor_length
              : request_length;  // Truncate to descriptor length at most

      while (descriptor_length > 0) {
        while (!(UEINTX & (1 << TXINI)))
          ;  // Wait for banks to be ready for data transmission
        if (UEINTX & (1 << RXOUTI))
          return;  // If there is another packet, exit to handle it

        uint8_t thisPacket =
            descriptor_length > 32
                ? 32
                : descriptor_length;  // Make sure that the packet we are
                                      // getting is not too big to fit in the
                                      // endpoint

        for (int i = 0; i < thisPacket; i++) {
          UEDATX = pgm_read_byte(
              descriptor +
              i);  // Send the descriptor over UEDATX, use pgmspace functions
                   // because the descriptors are stored in flash
        }

        descriptor_length -= thisPacket;
        descriptor += thisPacket;
        UEINTX &= ~(1 << TXINI);
      }
      return;
    }

    if (bRequest == SET_CONFIGURATION &&
        bmRequestType ==
            0) {  // Refer to USB Spec 9.4.7 - This is the configuration request
                  // to place the device into address mode
      usb_config_status = wValue;
      UEINTX &= ~(1 << TXINI);
      UENUM = KEYBOARD_ENDPOINT_NUM;
      UECONX = 1;
      UECFG0X = 0b11000001;  // EPTYPE Interrupt IN
      UECFG1X = 0b00000110;  // Dual Bank Endpoint, 8 Bytes, allocate memory
      UERST = 0x1E;          // Reset all of the endpoints
      UERST = 0;
      return;
    }

    if (bRequest == SET_ADDRESS) {
      UEINTX &= ~(1 << TXINI);
      while (!(UEINTX & (1 << TXINI)))
        ;  // Wait until the banks are ready to be filled

      UDADDR = wValue | (1 << ADDEN);  // Set the device address
      return;
    }

    if (bRequest == GET_CONFIGURATION &&
        bmRequestType == 0x80) {  // GET_CONFIGURATION is the host trying to get
                                  // the current config status of the device
      while (!(UEINTX & (1 << TXINI)))
        ;  // Wait until the banks are ready to be filled
      UEDATX = usb_config_status;
      UEINTX &= ~(1 << TXINI);
      return;
    }

    if (bRequest == GET_STATUS) {
      while (!(UEINTX & (1 << TXINI)))
        ;
      UEDATX = 0;
      UEDATX = 0;
      UEINTX &= ~(1 << TXINI);
      return;
    }

    if (wIndex == 0) {
      if (bmRequestType == 0xA1) {  // HID GET requests
        if (bRequest == GET_REPORT) {
          while (!(UEINTX & (1 << TXINI)))
            ;
          UEDATX = 0;  // buttons
          UEDATX = 0;  // dial delta
          UEINTX &= ~(1 << TXINI);
          return;
        }
        if (bRequest == GET_IDLE) {
          while (!(UEINTX & (1 << TXINI)))
            ;
          UEDATX = 0;
          UEINTX &= ~(1 << TXINI);
          return;
        }
      }
      if (bmRequestType == 0x21) {  // HID SET requests
        if (bRequest == SET_IDLE) {
          UEINTX &= ~(1 << TXINI);  // ACK, ignore value — no idle for relative device
          return;
        }
      }
    }
  }
  PORTC = 0xFF;
  UECONX |= (1 << STALLRQ) |
            (1 << EPEN);  // The host made an invalid request or there was an
                          // error with one of the request parameters
}
