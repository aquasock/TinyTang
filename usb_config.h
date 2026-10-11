/*
 * TinyTang — CherryUSB configuration.
 *
 * SPDX-License-Identifier: MIT
 *
 * CherryUSB includes "usb_config.h" from the application.  This takes
 * CherryUSB's own defaults (cherryusb_config_template.h, in the SDK) and
 * changes what upstream's BL616 USB console does (bsp/common/usb_app/
 * usb_config.h in Bouffalo SDK 2.3.36): the console is this USB device, so
 * CherryUSB's diagnostics print nowhere, and the BL616's port runs at high
 * speed, which the console's 512-byte packets need (USB-001).
 */
#ifndef TINYTANG_USB_CONFIG_H
#define TINYTANG_USB_CONFIG_H

#include "cherryusb_config_template.h"

#undef CONFIG_USB_PRINTF
#define CONFIG_USB_PRINTF(...) ((void)0)
#undef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL (-1)

#define CONFIG_USB_HS

#endif /* TINYTANG_USB_CONFIG_H */
