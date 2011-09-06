#ifndef __USB_GADGET_DESCRIPTORS_H__
#define __USB_GADGET_DESCRIPTORS_H__

/* Thanks to NetChip Technologies for donating this product ID.
 *
 * DO NOT REUSE THESE IDs with any protocol-incompatible driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */
#define DRIVER_VENDOR_NUM	0x0525		/* NetChip */
#define DRIVER_ISO_PRODUCT_NUM	0xa4a3		/* user mode iso sink/src */
#define DRIVER_PRODUCT_NUM	0xa4a4		/* user mode sink/src */

/* NOTE:  these IDs don't imply endpoint numbering; host side drivers
 * should use endpoint descriptors, or perhaps bcdDevice, to configure
 * such things.  Other product IDs could have different policies.
 */

/*-------------------------------------------------------------------------*/

/* these descriptors are modified based on what controller we find */

#define	STRINGID_MFGR		1
#define	STRINGID_PRODUCT	2
#define	STRINGID_SERIAL		0 //No serial id in this version
#define	STRINGID_CONFIG		4
#define	STRINGID_INTERFACE	5

static struct usb_device_descriptor 
device_desc = {
  .bLength =		sizeof device_desc,
  .bDescriptorType =	USB_DT_DEVICE,

  .bcdUSB =		__constant_cpu_to_le16 (0x0200),
  .bDeviceClass =		USB_CLASS_VENDOR_SPEC,
  .bDeviceSubClass =	0,
  .bDeviceProtocol =	0,
  // .bMaxPacketSize0 ... set by gadgetfs
  .idVendor =		__constant_cpu_to_le16 (DRIVER_VENDOR_NUM),
  .idProduct =		__constant_cpu_to_le16 (DRIVER_PRODUCT_NUM),
  .iManufacturer =	STRINGID_MFGR,
  .iProduct =		STRINGID_PRODUCT,
  .iSerialNumber =	STRINGID_SERIAL,
  .bNumConfigurations =	1,
};

#define	MAX_USB_POWER		1
#define	CONFIG_VALUE		1

static const struct usb_config_descriptor 
config = {
  .bLength =		sizeof config,
  .bDescriptorType =	USB_DT_CONFIG,

  /* must compute wTotalLength ... */
  .bNumInterfaces =	1,
  .bConfigurationValue =	CONFIG_VALUE,
  .iConfiguration =	STRINGID_CONFIG,
  .bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
  .bMaxPower =		(MAX_USB_POWER + 1) / 2,
};

static struct usb_interface_descriptor
gst_usb_intf = {
  .bLength =		sizeof gst_usb_intf,
  .bDescriptorType =	USB_DT_INTERFACE,

  .bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
  .iInterface =		STRINGID_INTERFACE,
};

/* Full speed configurations are used for full-speed only devices as
 * well as dual-speed ones (the only kind with high speed support).
 */

static struct usb_endpoint_descriptor
fs_stream_desc = {
  .bLength =		USB_DT_ENDPOINT_SIZE,
  .bDescriptorType =	USB_DT_ENDPOINT,

  .bmAttributes =		USB_ENDPOINT_XFER_BULK,
  /* NOTE some controllers may need FS bulk max packet size
   * to be smaller.  it would be a chip-specific option.
   */
  .wMaxPacketSize =	__constant_cpu_to_le16 (64),
};

static struct usb_endpoint_descriptor
fs_evup_desc = {
  .bLength =		USB_DT_ENDPOINT_SIZE,
  .bDescriptorType =	USB_DT_ENDPOINT,

  .bmAttributes =		USB_ENDPOINT_XFER_BULK,
  .wMaxPacketSize =	__constant_cpu_to_le16 (64),
};

static struct usb_endpoint_descriptor
fs_evdown_desc = {
  .bLength =		USB_DT_ENDPOINT_SIZE,
  .bDescriptorType =	USB_DT_ENDPOINT,

  .bmAttributes =		USB_ENDPOINT_XFER_BULK,
  .wMaxPacketSize =	__constant_cpu_to_le16 (64),
};


static const struct usb_endpoint_descriptor *fs_eps [3] = {
  &fs_stream_desc,
  &fs_evup_desc,
  &fs_evdown_desc,
};


/* High speed configurations are used only in addition to a full-speed
 * ones ... since all high speed devices support full speed configs.
 * Of course, not all hardware supports high speed configurations.
 */

static struct usb_endpoint_descriptor
hs_stream_desc = {
  .bLength =		USB_DT_ENDPOINT_SIZE,
  .bDescriptorType =	USB_DT_ENDPOINT,

  .bmAttributes =		USB_ENDPOINT_XFER_BULK,
  .wMaxPacketSize =	__constant_cpu_to_le16 (512),
  .bInterval =		1, //send one NAK every microframe
};

static struct usb_endpoint_descriptor
hs_evup_desc = {
  .bLength =		USB_DT_ENDPOINT_SIZE,
  .bDescriptorType =	USB_DT_ENDPOINT,

  .bmAttributes =		USB_ENDPOINT_XFER_BULK,
  .wMaxPacketSize =	__constant_cpu_to_le16 (512),
  /* In bulk transfers .bInterval is only used on OUT or control */
};

static struct usb_endpoint_descriptor
hs_evdown_desc = {
  .bLength =		USB_DT_ENDPOINT_SIZE,
  .bDescriptorType =	USB_DT_ENDPOINT,

  .bmAttributes =		USB_ENDPOINT_XFER_BULK,
  .wMaxPacketSize =	__constant_cpu_to_le16 (512),
  .bInterval =		1, //send one NAK every microframe
};

static const struct usb_endpoint_descriptor *hs_eps [] = {
  &hs_stream_desc,
  &hs_evup_desc,
  &hs_evdown_desc,
};


/*-------------------------------------------------------------------------*/

static char serial [64];

static struct usb_string stringtab [] = {
  { STRINGID_MFGR,	"(c) RidgeRun, 2011", },
  { STRINGID_PRODUCT,	"Gstreamer USB plugin", },
  { STRINGID_SERIAL,	serial, },
  { STRINGID_CONFIG,	"Stream raw data across USB link", },
  { STRINGID_INTERFACE,	"UsbSrc/UsbSink", },
};

static struct usb_gadget_strings strings = {
  .language =	0x0409,		/* "en-us" */
  .strings =	stringtab,
};

/*-------------------------------------------------------------------------*/


#endif /* __USB_GSDGET_DESCRIPTORS_H__ */
