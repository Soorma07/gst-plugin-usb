/*
 * Copyright (C) 2011 RidgeRun
 */

#ifndef __GST_USB_SINK_H__
#define __GST_USB_SINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gst/dataprotocol/dataprotocol.h>

#include "usbhost.h"
#include "gstusbmessages.h"
#include "gstusbclock.h"

G_BEGIN_DECLS

#define GST_TYPE_USB_SINK \
  (gst_usb_sink_get_type())
#define GST_USB_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_USB_SINK,GstUsbSink))
#define GST_USB_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_USB_SINK,GstUsbSinkClass))
#define GST_IS_USB_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_USB_SINK))
#define GST_IS_USB_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_USB_SINK))

typedef struct _GstUsbSink      GstUsbSink;
typedef struct _GstUsbSinkClass GstUsbSinkClass;

struct _GstUsbSink
{
  GstBaseSink parent;

  /* Properties */
  gboolean usbsync;
  
  usb_host *host;
  
  /* Used in get and set caps accross usb link */
  GstCaps *caps;
  gboolean emptycaps;
  
  /* Vars that aids sync */
  gboolean play;
  GstClockTimeDiff sync;

  /* Lock to prevent the state to change while working */
  GMutex *state_lock;

  /* Clock implementation */
  GstClock *provided_clock;
  GstClockTime gadgetclock;
};

struct _GstUsbSinkClass 
{
  GstBaseSinkClass parent_class;
};

GType gst_usb_sink_get_type (void);

G_END_DECLS

#endif /* __GST_USB_SINK_H__ */
