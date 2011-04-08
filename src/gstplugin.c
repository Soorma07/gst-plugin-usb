/*
 * GStreamer
 * Copyright (C) 2011 RidgeRun
 *
 */

#include <config.h>
#include <gst/gst.h>
#include "gstusbsink.h"
#include "gstusbsrc.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "usbsink", GST_RANK_NONE,
      GST_TYPE_USBSINK)) {
    return FALSE;
  }

  if (!gst_element_register (plugin, "usbsrc", GST_RANK_NONE,
      GST_TYPE_USB_SRC)) {
    return FALSE;
  }

  return TRUE;
}

/* gstreamer looks for this structure to register plugins
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "usb",
    "USB plugin",
    plugin_init,
    PACKAGE_VERSION,
    "BSD",
    "RidgeRun",
    "http://www.ridgerun.com/"
)
