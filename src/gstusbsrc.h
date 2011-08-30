/*
 * Copyright (C) 2011 RidgeRun
 */

#ifndef __GST_USB_SRC_H__
#define __GST_USB_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/dataprotocol/dataprotocol.h>
#include "usbgadget.h"
#include "gstusbmessages.h"

G_BEGIN_DECLS

#define GST_TYPE_USB_SRC \
  (gst_usb_src_get_type())
#define GST_USB_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_USB_SRC,GstUsbSrc))
#define GST_USB_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_USB_SRC,GstUsbSrcClass))
#define GST_IS_USB_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_USB_SRC))
#define GST_IS_USB_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_USB_SRC))

typedef struct _GstUsbSrc      GstUsbSrc;
typedef struct _GstUsbSrcClass GstUsbSrcClass;

struct _GstUsbSrc
{
  GstPushSrc parent;
  usb_gadget *gadget;
  gboolean play;

  /* block device when busy */
  GMutex  *state_lock;
};

struct _GstUsbSrcClass 
{
  GstPushSrcClass parent_class;
};

GType gst_usb_src_get_type (void);

G_END_DECLS

#endif /* __GST_USB_SRC_H__ */
