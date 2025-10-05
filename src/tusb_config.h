// File: src/tusb_config.h
// TinyUSB Konfigurationsdatei für den Raspberry Pi Pico als USB Host
// Projekt: Roland Mouse Emulator
// -----------------------------------------------------------------------------

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// BOARD CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUSB_MCU              OPT_MCU_RP2040
#define CFG_TUSB_OS               OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define BOARD_TUH_RHPORT          0

//--------------------------------------------------------------------
// HOST CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUH_HUB               1        // Unterstützung für USB-Hubs
#define CFG_TUH_HID_MOUSE         1        // HID-Maus
#define CFG_TUH_HID_KEYBOARD      0
#define CFG_TUH_HID_GENERIC       0

#define CFG_TUH_DEVICE_MAX        4        // max. Geräte
#define CFG_TUH_ENUMERATION_BUFSIZE 256

//--------------------------------------------------------------------
// MEMORY CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))

#ifdef __cplusplus
 }
#endif

#endif // _TUSB_CONFIG_H_
